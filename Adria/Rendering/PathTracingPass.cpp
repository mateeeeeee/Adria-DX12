#include "PathTracingPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "OIDNDenoiserPass.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxStateObject.h"
#include "Graphics/GfxCommon.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<Int> Denoiser("r.PathTracing.Denoiser", DenoiserType_None, "What denoiser will path tracer use: 0 - None, 1 - OIDN, 2 - SVGF");

	PathTracingPass::PathTracingPass(GfxDevice* gfx, Uint32 width, Uint32 height)
		: gfx(gfx), width(width), height(height)
	{
		is_supported = gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1);
		if (IsSupported())
		{
			CreateStateObject();
			denoiser_pass.reset(CreateDenoiser(gfx, (DenoiserType)Denoiser.Get()));
			OnResize(width, height);
			ShaderManager::GetLibraryRecompiledEvent().AddMember(&PathTracingPass::OnLibraryRecompiled, *this);
		}
	}
	PathTracingPass::~PathTracingPass() = default;

	void PathTracingPass::AddPass(RenderGraph& rg)
	{
		if (!IsSupported()) return;

		Bool const denoiser_active = denoiser_pass->GetType() != DenoiserType_None;
		
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct PathTracingPassData
		{
			RGTextureReadWriteId output;
			RGTextureReadWriteId accumulation;
			RGTextureReadWriteId albedo;
			RGTextureReadWriteId normal;
		};

		rg.ImportTexture(RG_NAME(AccumulationTexture), accumulation_texture.get());
		if (denoiser_active)
		{
			rg.ImportTexture(RG_NAME(PT_Albedo), denoiser_albedo_texture.get());
			rg.ImportTexture(RG_NAME(PT_Normal), denoiser_normal_texture.get());
		}
		rg.AddPass<PathTracingPassData>("Path Tracing Pass",
			[=](PathTracingPassData& data, RGBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_NAME(PT_Output), render_target_desc);

				data.output = builder.WriteTexture(RG_NAME(PT_Output));
				data.accumulation = builder.WriteTexture(RG_NAME(AccumulationTexture));
				if (denoiser_active)
				{
					data.albedo = builder.WriteTexture(RG_NAME(PT_Albedo));
					data.normal = builder.WriteTexture(RG_NAME(PT_Normal));
				}
			},
			[=](PathTracingPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadWriteTexture(data.accumulation),
					ctx.GetReadWriteTexture(data.output),
					denoiser_active ? ctx.GetReadWriteTexture(data.albedo) : gfxcommon::GetCommonView(GfxCommonViewType::NullTexture2D_UAV),
					denoiser_active ? ctx.GetReadWriteTexture(data.normal) : gfxcommon::GetCommonView(GfxCommonViewType::NullTexture2D_UAV)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct PathTracingConstants
				{
					Int32   bounce_count;
					Int32   accumulated_frames;
					Uint32  accum_idx;
					Uint32  output_idx;
					Uint32  albedo_idx;
					Uint32  normal_idx;
				} constants =
				{
					.bounce_count = max_bounces, .accumulated_frames = accumulated_frames,
					.accum_idx = i + 0, .output_idx = i + 1, .albedo_idx = i + 2, .normal_idx = i + 3
				};

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				auto& table = cmd_list->SetStateObject(denoiser_active ? path_tracing_so_write_gbuffer.get() : path_tracing_so.get());
				table.SetRayGenShader("PT_RayGen");
				cmd_list->DispatchRays(width, height);

			}, RGPassType::Compute, RGPassFlags::None);

		if (denoiser_active)
		{
			denoiser_pass->AddPass(rg);
		}
		++accumulated_frames;
	}

	void PathTracingPass::OnResize(Uint32 w, Uint32 h)
	{
		if (!IsSupported()) return;

		width = w, height = h;
		CreateAccumulationTexture();
		CreateDenoiserTextures();
	}

	Bool PathTracingPass::IsSupported() const
	{
		return is_supported;
	}

	void PathTracingPass::Reset()
	{
		accumulated_frames = 0;
		denoiser_pass->Reset();
	}

	void PathTracingPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Path Tracing Settings", ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Combo("Denoiser Type", Denoiser.GetPtr(), "None\0OIDN\0SVGF\0", 3))
					{
						denoiser_pass.reset(CreateDenoiser(gfx, (DenoiserType)Denoiser.Get()));
						OnResize(width, height);
					}
					ImGui::SliderInt("Max bounces", &max_bounces, 1, 8);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_Renderer
		);
	}

	RGResourceName PathTracingPass::GetFinalOutput() const
	{
		return Denoiser.Get() != DenoiserType_None ? RG_NAME(PT_Output) : RG_NAME(PT_Output);
	}

	void PathTracingPass::CreateStateObject()
	{
		GfxShaderKey pt_shader_key(LIB_PathTracing);
		GfxShader const& pt_blob = GetGfxShader(pt_shader_key);
		path_tracing_so.reset(CreateStateObjectCommon(pt_shader_key));

		pt_shader_key.AddDefine("WRITE_GBUFFER", "1");
		GfxShader const& pt_blob_write_gbuffer = GetGfxShader(pt_shader_key);
		path_tracing_so_write_gbuffer.reset(CreateStateObjectCommon(pt_shader_key));
	}

	GfxStateObject* PathTracingPass::CreateStateObjectCommon(GfxShaderKey const& shader_key)
	{
		GfxShader const& pt_blob = GetGfxShader(shader_key);
		GfxStateObjectBuilder pt_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = pt_blob.GetSize();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = pt_blob.GetData();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			pt_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG pt_shader_config{};
			pt_shader_config.MaxPayloadSizeInBytes = sizeof(Float);
			pt_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			pt_state_object_builder.AddSubObject(pt_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = gfx->GetCommonRootSignature();
			pt_state_object_builder.AddSubObject(global_root_sig);

			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 3;
			pt_state_object_builder.AddSubObject(pipeline_config);
		}
		return pt_state_object_builder.CreateStateObject(gfx);
	}

	void PathTracingPass::OnLibraryRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == LIB_PathTracing) CreateStateObject();
	}

	void PathTracingPass::CreateAccumulationTexture()
	{
		if (!accumulation_texture || accumulation_texture->GetWidth() != width || accumulation_texture->GetHeight() != height)
		{
			GfxTextureDesc accum_desc{};
			accum_desc.width = width;
			accum_desc.height = height;
			accum_desc.format = GfxFormat::R32G32B32A32_FLOAT;
			accum_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
			accum_desc.initial_state = GfxResourceState::ComputeUAV;
			accumulation_texture = gfx->CreateTexture(accum_desc);
		}
	}

	void PathTracingPass::CreateDenoiserTextures()
	{
		if (denoiser_pass->GetType() != DenoiserType_None && (!denoiser_albedo_texture || denoiser_albedo_texture->GetWidth() != width || denoiser_albedo_texture->GetHeight() != height))
		{
			GfxTextureDesc history_desc{};
			history_desc.width = width;
			history_desc.height = height;
			history_desc.format = GfxFormat::R16G16B16A16_FLOAT;
			history_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
			history_desc.initial_state = GfxResourceState::ComputeUAV;
			denoiser_albedo_texture = gfx->CreateTexture(history_desc);
			denoiser_normal_texture = gfx->CreateTexture(history_desc);
		}
		else if (denoiser_pass->GetType() == DenoiserType_None)
		{
			denoiser_albedo_texture.reset();
			denoiser_normal_texture.reset();
		}
	}

}





