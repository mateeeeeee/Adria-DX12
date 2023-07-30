#include "TAAPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxRingDescriptorAllocator.h"

namespace adria
{

	TAAPass::TAAPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName TAAPass::AddPass(RenderGraph& rg, RGResourceName input, RGResourceName history)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		RGResourceName last_resource = input;

		struct TAAPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId history;
			RGTextureReadOnlyId velocity;
			RGTextureReadWriteId output;
		};

		rg.AddPass<TAAPassData>("TAA Pass",
			[=](TAAPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc taa_desc{};
				taa_desc.width = width;
				taa_desc.height = height;
				taa_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(TAAOutput), taa_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(TAAOutput));
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.history = builder.ReadTexture(RG_RES_NAME(HistoryBuffer), ReadAccess_PixelShader);
				data.velocity = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_PixelShader);
			},
			[=](TAAPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				uint32 i = gfx->AllocateDescriptorsGPU(4).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.input));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadOnlyTexture(data.history));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 2), ctx.GetReadOnlyTexture(data.velocity));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 3), ctx.GetReadWriteTexture(data.output));

				struct TAAConstants
				{
					uint32 scene_idx;
					uint32 prev_scene_idx;
					uint32 velocity_idx;
					uint32 output_idx;
				} constants =
				{
					.scene_idx = i, .prev_scene_idx = i + 1, .velocity_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::TAA));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		return RG_RES_NAME(TAAOutput);
	}

	void TAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}