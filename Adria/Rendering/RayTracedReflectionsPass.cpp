#include "RayTracedReflectionsPass.h"
#include "BlackboardData.h"
#include "ShaderCache.h"
#include "PSOCache.h"

#include "../Graphics/GfxShader.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	RayTracedReflectionsPass::RayTracedReflectionsPass(GfxDevice* gfx, uint32 width, uint32 height)
		: gfx(gfx), width(width), height(height), blur_pass(width, height)
	{
		ID3D12Device* device = gfx->GetDevice();
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
		HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		is_supported = features5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
		if (IsSupported())
		{
			CreateStateObject();
			ShaderCache::GetLibraryRecompiledEvent().AddMember(&RayTracedReflectionsPass::OnLibraryRecompiled, *this);
		}
	}

	void RayTracedReflectionsPass::AddPass(RenderGraph& rg)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		struct RayTracedReflectionsPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadWriteId output;

			RGBufferReadOnlyId vb;
			RGBufferReadOnlyId ib;
			RGBufferReadOnlyId geo;
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
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal));

				data.vb = builder.ReadBuffer(RG_RES_NAME(BigVertexBuffer));
				data.ib = builder.ReadBuffer(RG_RES_NAME(BigIndexBuffer));
				data.geo = builder.ReadBuffer(RG_RES_NAME(BigGeometryBuffer));
			},
			[=](RayTracedReflectionsPassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				uint32 i = descriptor_allocator->Allocate(5).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyBuffer(data.vb));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadOnlyBuffer(data.ib));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 4), ctx.GetReadOnlyBuffer(data.geo));

				struct RayTracedReflectionsConstants
				{
					float   roughness_scale;
					uint32  depth_idx;
					uint32  output_idx;
					uint32  vertices_idx;
					uint32  indices_idx;
					uint32  geo_infos_idx;
				} constants =
				{
					.roughness_scale = reflection_roughness_scale,
					.depth_idx = i + 0, .output_idx = i + 1,
					.vertices_idx = i + 2, .indices_idx = i + 3, .geo_infos_idx = i + 4
				};
				auto& table = cmd_list->SetStateObject(ray_traced_reflections.Get());
				table.SetRayGenShader("RTR_RayGen");
				table.AddMissShader("RTR_Miss", 0);
				table.AddHitGroup("RTRClosestHitGroupPrimaryRay", 0);
				table.AddHitGroup("RTRClosestHitGroupReflectionRay", 1);

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->DispatchRays(width, height);
			}, RGPassType::Compute, RGPassFlags::None);

		blur_pass.AddPass(rg, RG_RES_NAME(RTR_OutputNoisy), RG_RES_NAME(RTR_Output), "RTR Denoise");

		AddGUI([&]()
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
			rtr_shader_config.MaxPayloadSizeInBytes = sizeof(float) * 5;
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

			closesthit_group.ClosestHitShaderImport = L"RTR_ClosestHitReflectionRay";
			closesthit_group.HitGroupExport = L"RTRClosestHitGroupReflectionRay";
			rtr_state_object_builder.AddSubObject(closesthit_group);

			ray_traced_reflections.Attach(rtr_state_object_builder.CreateStateObject(device));
		}
	}

	void RayTracedReflectionsPass::OnLibraryRecompiled(GfxShaderID shader)
	{
		if (shader == LIB_Reflections) CreateStateObject();
	}

}


