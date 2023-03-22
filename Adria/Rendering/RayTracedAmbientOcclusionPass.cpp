#include "RayTracedAmbientOcclusionPass.h"
#include "BlackboardData.h"
#include "ShaderCache.h"
#include "PSOCache.h" 

#include "../Graphics/GfxShader.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	RayTracedAmbientOcclusionPass::RayTracedAmbientOcclusionPass(GfxDevice* gfx, uint32 width, uint32 height)
		: gfx(gfx), width(width), height(height), blur_pass(width, height)
	{
		ID3D12Device* device = gfx->GetDevice();
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
		HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		is_supported = features5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
		if (IsSupported())
		{
			CreateStateObject();
			ShaderCache::GetLibraryRecompiledEvent().AddMember(&RayTracedAmbientOcclusionPass::OnLibraryRecompiled, *this);
		}
	}

	void RayTracedAmbientOcclusionPass::AddPass(RenderGraph& rg)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		struct RayTracedAmbientOcclusionPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadWriteId output;
		};

		rg.AddPass<RayTracedAmbientOcclusionPassData>("Ray Traced Ambient Occlusion Pass",
			[=](RayTracedAmbientOcclusionPassData& data, RGBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = GfxFormat::R8_UNORM;
				builder.DeclareTexture(RG_RES_NAME(RTAO_Output), desc);

				data.output = builder.WriteTexture(RG_RES_NAME(RTAO_Output));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				data.normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
			},
			[=](RayTracedAmbientOcclusionPassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				uint32 i = descriptor_allocator->Allocate(3).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.normal));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadWriteTexture(data.output));

				struct RayTracedAmbientOcclusionConstants
				{
					uint32  depth_idx;
					uint32  gbuf_normals_idx;
					uint32  output_idx;
					float   ao_radius;
				} constants =
				{
					.depth_idx = i + 0, .gbuf_normals_idx = i + 1, .output_idx = i + 2,
					.ao_radius = ao_radius
				};

				auto& table = cmd_list->SetStateObject(ray_traced_ambient_occlusion.Get());
				table.SetRayGenShader("RTAO_RayGen");
				table.AddMissShader("RTAO_Miss", 0);
				table.AddHitGroup("RTAOAnyHitGroup", 0);

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->DispatchRays(width, height);
			}, RGPassType::Compute, RGPassFlags::None);

		blur_pass.AddPass(rg, RG_RES_NAME(RTAO_Output), RG_RES_NAME(AmbientOcclusion));
		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("RTAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderFloat("Radius", &ao_radius, 1.0f, 16.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}

	void RayTracedAmbientOcclusionPass::OnResize(uint32 w, uint32 h)
	{
		if (!IsSupported()) return;

		width = w, height = h;
		blur_pass.OnResize(w, h);
	}

	bool RayTracedAmbientOcclusionPass::IsSupported() const
	{
		return is_supported;
	}

	void RayTracedAmbientOcclusionPass::CreateStateObject()
	{
		ID3D12Device5* device = gfx->GetDevice();
		GfxShader const& rtao_blob = ShaderCache::GetShader(LIB_AmbientOcclusion);

		GfxStateObjectBuilder rtao_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rtao_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rtao_blob.GetPointer();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			rtao_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG rtao_shader_config{};
			rtao_shader_config.MaxPayloadSizeInBytes = 4;	//bool in hlsl is 4 bytes
			rtao_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rtao_state_object_builder.AddSubObject(rtao_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = gfx->GetCommonRootSignature();
			rtao_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 1;
			rtao_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC anyhit_group{};
			anyhit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			anyhit_group.AnyHitShaderImport = L"RTAO_AnyHit";
			anyhit_group.HitGroupExport = L"RTAOAnyHitGroup";
			rtao_state_object_builder.AddSubObject(anyhit_group);

			ray_traced_ambient_occlusion.Attach(rtao_state_object_builder.CreateStateObject(device));
		}

	}

	void RayTracedAmbientOcclusionPass::OnLibraryRecompiled(GfxShaderID shader)
	{
		if (shader == LIB_AmbientOcclusion) CreateStateObject();
	}

}


