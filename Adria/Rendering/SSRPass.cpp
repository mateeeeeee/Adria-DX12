#include "SSRPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

namespace adria
{
	SSRPass::SSRPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName SSRPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		RGResourceName last_resource = input;
		struct SSRPassData
		{
			RGTextureReadOnlyId normals;
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};

		rg.AddPass<SSRPassData>("SSR Pass",
			[=](SSRPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc ssr_output_desc{};
				ssr_output_desc.width = width;
				ssr_output_desc.height = height;
				ssr_output_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(SSR_Output), ssr_output_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(SSR_Output));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
				data.normals = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](SSRPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				
				uint32 i = (uint32)descriptor_allocator->AllocateRange(4);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.normals), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyTexture(data.input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct SSRConstants
				{
					float ssr_ray_step;
					float ssr_ray_hit_threshold;

					uint32 depth_idx;
					uint32 normal_idx;
					uint32 scene_idx;
					uint32 output_idx;
				} constants = 
				{
					.ssr_ray_step = params.ssr_ray_step, .ssr_ray_hit_threshold = params.ssr_ray_hit_threshold,
					.depth_idx = i, .normal_idx = i + 1, .scene_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::SSR));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 6, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute);

		AddGUI([&]() {
			if (ImGui::TreeNodeEx("Screen-Space Reflections", 0))
			{
				ImGui::SliderFloat("Ray Step", &params.ssr_ray_step, 1.0f, 3.0f);
				ImGui::SliderFloat("Ray Hit Threshold", &params.ssr_ray_hit_threshold, 0.25f, 5.0f);

				ImGui::TreePop();
				ImGui::Separator();
			}
			});
		return RG_RES_NAME(SSR_Output);
	}

	void SSRPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

