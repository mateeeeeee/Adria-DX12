#include "RayTracedReflectionsPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "PostProcessor.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxStateObject.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<bool> RTR("r.RTR", true, "0 - Disabled, 1 - Enabled");
	
	RayTracedReflectionsPass::RayTracedReflectionsPass(GfxDevice* gfx, uint32 width, uint32 height)
		: gfx(gfx), width(width), height(height), blur_pass(gfx), copy_to_texture_pass(gfx, width, height)
	{
		is_supported = gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1);
		if (IsSupported())
		{
			CreateStateObject();
			ShaderManager::GetLibraryRecompiledEvent().AddMember(&RayTracedReflectionsPass::OnLibraryRecompiled, *this);
		}
	}
	RayTracedReflectionsPass::~RayTracedReflectionsPass() = default;

	void RayTracedReflectionsPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct RayTracedReflectionsPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadOnlyId diffuse;
			RGTextureReadWriteId output;
		};

		rg.AddPass<RayTracedReflectionsPassData>("Ray Traced Reflections Pass",
			[=](RayTracedReflectionsPassData& data, RGBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R8G8B8A8_UNORM;
				builder.DeclareTexture(RG_NAME(RTR_OutputNoisy), desc);

				data.output = builder.WriteTexture(RG_NAME(RTR_OutputNoisy));
				data.normal = builder.ReadTexture(RG_NAME(GBufferNormal));
				data.diffuse = builder.ReadTexture(RG_NAME(GBufferAlbedo));
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
			},
			[=](RayTracedReflectionsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadOnlyTexture(data.normal),
					ctx.GetReadOnlyTexture(data.diffuse),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct RayTracedReflectionsConstants
				{
					float   roughness_scale;
					uint32  depth_idx;
					uint32  normal_idx;
					uint32  albedo_idx;
					uint32  output_idx;
				} constants =
				{
					.roughness_scale = reflection_roughness_scale,
					.depth_idx = i + 0, .normal_idx = i + 1, .albedo_idx = i + 2, .output_idx = i + 3
				};
				auto& table = cmd_list->SetStateObject(ray_traced_reflections_so.get());
				table.SetRayGenShader("RTR_RayGen");
				table.AddMissShader("RTR_Miss", 0);
				table.AddHitGroup("RTRClosestHitGroupPrimaryRay", 0);

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->DispatchRays(width, height);
			}, RGPassType::Compute, RGPassFlags::None);
		
		blur_pass.AddPass(rg, RG_NAME(RTR_OutputNoisy), RG_NAME(RTR_Output), "RTR Denoise");
		copy_to_texture_pass.AddPass(rg, postprocessor->GetFinalResource(), RG_NAME(RTR_Output), BlendMode::AdditiveBlend);
	}

	void RayTracedReflectionsPass::OnResize(uint32 w, uint32 h)
	{
		if (!IsSupported()) return;
		width = w, height = h;
		copy_to_texture_pass.OnResize(w, h);
	}

	bool RayTracedReflectionsPass::IsEnabled(PostProcessor const*) const
	{
		return RTR.Get();
	}

	void RayTracedReflectionsPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Ray Traced Reflection", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Enable RTR", RTR.GetPtr());
					if (RTR.Get())
					{
						ImGui::SliderFloat("Roughness scale", &reflection_roughness_scale, 0.0f, 0.25f);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Reflection);
	}

	bool RayTracedReflectionsPass::IsSupported() const
	{
		return is_supported;
	}

	void RayTracedReflectionsPass::CreateStateObject()
	{
		GfxShader const& rtr_blob = GetGfxShader(LIB_Reflections);
		GfxStateObjectBuilder rtr_state_object_builder(6);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rtr_blob.GetSize();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rtr_blob.GetData();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			rtr_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG rtr_shader_config{};
			rtr_shader_config.MaxPayloadSizeInBytes = sizeof(float) * 4;
			rtr_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rtr_state_object_builder.AddSubObject(rtr_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = gfx->GetCommonRootSignature();
			rtr_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 2;
			rtr_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC closesthit_group{};
			closesthit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			closesthit_group.ClosestHitShaderImport = L"RTR_ClosestHitPrimaryRay";
			closesthit_group.HitGroupExport = L"RTRClosestHitGroupPrimaryRay";
			rtr_state_object_builder.AddSubObject(closesthit_group);

			//closesthit_group.ClosestHitShaderImport = L"RTR_ClosestHitReflectionRay";
			//closesthit_group.HitGroupExport = L"RTRClosestHitGroupReflectionRay";
			//rtr_state_object_builder.AddSubObject(closesthit_group);
		}
		ray_traced_reflections_so.reset(rtr_state_object_builder.CreateStateObject(gfx));
	}

	void RayTracedReflectionsPass::OnLibraryRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == LIB_Reflections) CreateStateObject();
	}

}


