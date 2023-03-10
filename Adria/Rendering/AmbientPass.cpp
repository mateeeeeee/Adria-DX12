#include "AmbientPass.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GraphicsCommon.h"
#include "../Math/Packing.h"
#include "../Editor/GUICommand.h"

namespace adria
{
	AmbientPass::AmbientPass(uint32 w, uint32 h)
		: width(w), height(h)
	{}

	void AmbientPass::AddPass(RenderGraph& rendergraph)
	{
		struct AmbientPassData
		{
			RGTextureReadOnlyId gbuffer_normal;
			RGTextureReadOnlyId gbuffer_albedo;
			RGTextureReadOnlyId gbuffer_emissive;
			RGTextureReadOnlyId depth_stencil;

			RGTextureReadOnlyId ambient_occlusion;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();
		rendergraph.AddPass<AmbientPassData>("Ambient Pass",
			[=](AmbientPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc hdr_desc{};
				hdr_desc.format = EFormat::R16G16B16A16_FLOAT;
				hdr_desc.width = width;
				hdr_desc.height = height;
				hdr_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(HDR_RenderTarget), hdr_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_RES_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
				data.depth_stencil = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				if (builder.IsTextureDeclared(RG_RES_NAME(AmbientOcclusion)))
					data.ambient_occlusion = builder.ReadTexture(RG_RES_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				else 
					data.ambient_occlusion.Invalidate();
			},
			[&](AmbientPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal),
					context.GetReadOnlyTexture(data.gbuffer_albedo), context.GetReadOnlyTexture(data.gbuffer_emissive), context.GetReadOnlyTexture(data.depth_stencil),
					data.ambient_occlusion.IsValid() ? context.GetReadOnlyTexture(data.ambient_occlusion) : gfxcommon::GetCommonView(ECommonViewType::NullTexture2D_SRV), 
					context.GetReadWriteTexture(data.output)};
				uint32 src_range_sizes[] = { 1,1,1,1,1,1 };
				uint32 i = (uint32)descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst = descriptor_allocator->GetHandle(i);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				FLOAT clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearUnorderedAccessViewFloat(descriptor_allocator->GetHandle(i + 5), context.GetReadWriteTexture(data.output),
					context.GetTexture(data.output.GetResourceId()).GetNative(), clear, 0, nullptr);

				struct AmbientConstants
				{
					uint32  color;

					uint32  normal_idx;
					uint32  diffuse_idx;
					uint32  emissive_idx;
					uint32  depth_idx;
					int32   ao_idx;
					uint32  output_idx;
				} constants =
				{
					.color = PackToUint(ambient_color),
					.normal_idx = i, .diffuse_idx = i + 1, .emissive_idx = i + 2,
					.depth_idx = i + 3, .ao_idx = -1, .output_idx = i + 5
				};
				if (data.ambient_occlusion.IsValid()) constants.ao_idx = static_cast<int32>(i + 4);

				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Ambient));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 7, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute);

		AddGUI([&]()
			{
				if (ImGui::TreeNode("Ambient"))
				{
					ImGui::ColorEdit3("Ambient Color", ambient_color);
					ImGui::Checkbox("IBL", &ibl);
					if (ibl) ibl = false;
					ImGui::TreePop();
				}
			}
		);
	}

	void AmbientPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}