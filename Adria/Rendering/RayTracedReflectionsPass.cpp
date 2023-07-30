#include "RayTracedReflectionsPass.h"
#include "BlackboardData.h"
#include "ShaderCache.h"
#include "PSOCache.h"

#include "Graphics/GfxShader.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{

	RayTracedReflectionsPass::RayTracedReflectionsPass(GfxDevice* gfx, uint32 width, uint32 height)
		: gfx(gfx), width(width), height(height), blur_pass(width, height)
	{
		is_supported = gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1);
		if (IsSupported())
		{
			CreateStateObject();
			ShaderCache::GetLibraryRecompiledEvent().AddMember(&RayTracedReflectionsPass::OnLibraryRecompiled, *this);
		}
	}

	void RayTracedReflectionsPass::AddPass(RenderGraph& rg)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();
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
				builder.DeclareTexture(RG_RES_NAME(RTR_OutputNoisy), desc);

				data.output = builder.WriteTexture(RG_RES_NAME(RTR_OutputNoisy));
				data.normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal));
				data.diffuse = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil));
			},
			[=](RayTracedReflectionsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				uint32 i = gfx->AllocateDescriptorsGPU(4).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.depth));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadOnlyTexture(data.normal));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 2), ctx.GetReadOnlyTexture(data.diffuse));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 3), ctx.GetReadWriteTexture(data.output));

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
				auto& table = cmd_list->SetStateObject(ray_traced_reflections.Get());
				table.SetRayGenShader("RTR_RayGen");
				table.AddMissShader("RTR_Miss", 0);
				table.AddHitGroup("RTRClosestHitGroupPrimaryRay", 0);

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->DispatchRays(width, height);
			}, RGPassType::Compute, RGPassFlags::None);

		blur_pass.AddPass(rg, RG_RES_NAME(RTR_OutputNoisy), RG_RES_NAME(RTR_Output), "RTR Denoise");

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx("Ray Traced Reflection", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderFloat("Roughness scale", &reflection_roughness_scale, 0.0f, 0.25f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

	void RayTracedReflectionsPass::OnResize(uint32 w, uint32 h)
	{
		if (!IsSupported()) return;
		width = w, height = h;
		blur_pass.OnResize(w, h);
	}

	bool RayTracedReflectionsPass::IsSupported() const
	{
		return is_supported;
	}

	void RayTracedReflectionsPass::CreateStateObject()
	{
		ID3D12Device5* device = gfx->GetDevice();
		GfxShader const& rtr_blob = ShaderCache::GetShader(LIB_Reflections);

		GfxStateObjectBuilder rtr_state_object_builder(6);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rtr_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rtr_blob.GetPointer();
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

			ray_traced_reflections.Attach(rtr_state_object_builder.CreateStateObject(device));
		}
	}

	void RayTracedReflectionsPass::OnLibraryRecompiled(GfxShaderID shader)
	{
		if (shader == LIB_Reflections) CreateStateObject();
	}

}


