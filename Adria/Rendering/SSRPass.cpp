#include "SSRPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{
	SSRPass::SSRPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName SSRPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		RGResourceName last_resource = input;
		struct SSRPassData
		{
			RGTextureReadOnlyId normals;
			RGTextureReadOnlyId roughness;
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
				ssr_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(SSR_Output), ssr_output_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(SSR_Output));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
				data.normals = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.roughness = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](SSRPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				uint32 i = gfx->AllocateDescriptorsGPU(5).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.depth));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadOnlyTexture(data.normals));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 2), ctx.GetReadOnlyTexture(data.roughness));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 3), ctx.GetReadOnlyTexture(data.input));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 4), ctx.GetReadWriteTexture(data.output));

				struct SSRConstants
				{
					float ssr_ray_step;
					float ssr_ray_hit_threshold;

					uint32 depth_idx;
					uint32 normal_idx;
					uint32 diffuse_idx;
					uint32 scene_idx;
					uint32 output_idx;
				} constants = 
				{
					.ssr_ray_step = params.ssr_ray_step, .ssr_ray_hit_threshold = params.ssr_ray_hit_threshold,
					.depth_idx = i, .normal_idx = i + 1, .diffuse_idx = i + 2, .scene_idx = i + 3, .output_idx = i + 4
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::SSR));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute);

		GUI_RunCommand([&]() {
			if (ImGui::TreeNodeEx("Screen-Space Reflections", 0))
			{
				ImGui::SliderFloat("Ray Step", &params.ssr_ray_step, 1.0f, 3.0f);
				ImGui::SliderFloat("Ray Hit Threshold", &params.ssr_ray_hit_threshold, 0.25f, 5.0f);

				ImGui::TreePop();
				ImGui::Separator();
			}
			}, GUICommandGroup_PostProcessing);
		return RG_RES_NAME(SSR_Output);
	}

	void SSRPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

