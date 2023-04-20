#include "FXAAPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	FXAAPass::FXAAPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void FXAAPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		struct FXAAPassData
		{
			RGTextureReadWriteId	output;
			RGTextureReadOnlyId		ldr;
		};

		rg.AddPass<FXAAPassData>("FXAA Pass",
			[=](FXAAPassData& data, RenderGraphBuilder& builder)
			{
				data.ldr = builder.ReadTexture(input, ReadAccess_NonPixelShader);
				ADRIA_ASSERT(builder.IsTextureDeclared(RG_RES_NAME(FinalTexture)));
				data.output = builder.WriteTexture(RG_RES_NAME(FinalTexture));
			},
			[=](FXAAPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				uint32 i = descriptor_allocator->Allocate(2).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.ldr));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output));
				
				struct SSRConstants
				{
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::FXAA));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void FXAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}
