#include "PathTracingPass.h"
#include "BlackboardData.h"
#include "ShaderCache.h"
#include "PSOCache.h" 

#include "../Graphics/GfxShader.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../Graphics/GfxLinearDynamicAllocator.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	PathTracingPass::PathTracingPass(GfxDevice* gfx, uint32 width, uint32 height)
		: gfx(gfx), width(width), height(height)
	{
		ID3D12Device* device = gfx->GetDevice();
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
		HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		is_supported = features5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
		if (IsSupported())
		{
			CreateStateObject();
			OnResize(width, height);
			ShaderCache::GetLibraryRecompiledEvent().AddMember(&PathTracingPass::OnLibraryRecompiled, *this);
		}
	}

	void PathTracingPass::AddPass(RenderGraph& rg)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		struct PathTracingPassData
		{
			RGTextureReadWriteId output;
			RGTextureReadWriteId accumulation;

			RGBufferReadOnlyId vb;
			RGBufferReadOnlyId ib;
			RGBufferReadOnlyId geo;
		};

		rg.ImportTexture(RG_RES_NAME(AccumulationTexture), accumulation_texture.get());
		rg.AddPass<PathTracingPassData>("Path Tracing Pass",
			[=](PathTracingPassData& data, RGBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(PT_Output), render_target_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(PT_Output));
				data.accumulation = builder.WriteTexture(RG_RES_NAME(AccumulationTexture));

				data.vb = builder.ReadBuffer(RG_RES_NAME(BigVertexBuffer));
				data.ib = builder.ReadBuffer(RG_RES_NAME(BigIndexBuffer));
				data.geo = builder.ReadBuffer(RG_RES_NAME(BigGeometryBuffer));
			},
			[=](PathTracingPassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				uint32 i = descriptor_allocator->Allocate(5).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadWriteTexture(data.accumulation));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyBuffer(data.vb));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadOnlyBuffer(data.ib));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 4), ctx.GetReadOnlyBuffer(data.geo));

				struct PathTracingConstants
				{
					int32   bounce_count;
					int32   accumulated_frames;
					uint32  accum_idx;
					uint32  output_idx;
					uint32  vertices_idx;
					uint32  indices_idx;
					uint32  geo_infos_idx;
				} constants =
				{
					.bounce_count = max_bounces, .accumulated_frames = accumulated_frames,
					.accum_idx = i + 0, .output_idx = i + 1,
					.vertices_idx = i + 2, .indices_idx = i + 3, .geo_infos_idx = i + 4
				};

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				auto& table = cmd_list->SetStateObject(path_tracing.Get());
				table.SetRayGenShader("PT_RayGen");
				cmd_list->DispatchRays(width, height);

			}, RGPassType::Compute, RGPassFlags::None);

		++accumulated_frames;
		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Path tracing", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderInt("Max bounces", &max_bounces, 1, 8);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);

	}

	void PathTracingPass::OnResize(uint32 w, uint32 h)
	{
		if (!IsSupported()) return;

		width = w, height = h;

		GfxTextureDesc accum_desc{};
		accum_desc.width = width;
		accum_desc.height = height;
		accum_desc.format = GfxFormat::R32G32B32A32_FLOAT;
		accum_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		accum_desc.initial_state = GfxResourceState::UnorderedAccess;
		accumulation_texture = std::make_unique<GfxTexture>(gfx, accum_desc);
	}

	bool PathTracingPass::IsSupported() const
	{
		return is_supported;
	}

	void PathTracingPass::Reset()
	{
		accumulated_frames = 0;
	}

	void PathTracingPass::CreateStateObject()
	{
		ID3D12Device5* device = gfx->GetDevice();
		GfxShader const& pt_blob = ShaderCache::GetShader(LIB_PathTracing);

		GfxStateObjectBuilder pt_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = pt_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = pt_blob.GetPointer();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			pt_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG pt_shader_config{};
			pt_shader_config.MaxPayloadSizeInBytes = sizeof(float) * 1;
			pt_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			pt_state_object_builder.AddSubObject(pt_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = gfx->GetCommonRootSignature();
			pt_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 3;
			pt_state_object_builder.AddSubObject(pipeline_config);

			//D3D12_HIT_GROUP_DESC closesthit_group{};
			//closesthit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			//closesthit_group.ClosestHitShaderImport = L"PT_ClosestHit";
			//closesthit_group.HitGroupExport = L"PT_HitGroup";
			//pt_state_object_builder.AddSubObject(closesthit_group);

			path_tracing.Attach(pt_state_object_builder.CreateStateObject(device));
		}
	}

	void PathTracingPass::OnLibraryRecompiled(GfxShaderID shader)
	{
		if (shader == LIB_PathTracing) CreateStateObject();
	}

}





