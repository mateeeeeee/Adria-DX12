#include "CopyToTexturePass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	void CopyToTexturePass::AddPass(RenderGraph& rendergraph, RGResourceName render_target, RGResourceName texture_src, BlendMode mode /*= EBlendMode::None*/)
	{
		struct CopyToTexturePassData
		{
			RGTextureReadOnlyId texture_src;
		};

		rendergraph.AddPass<CopyToTexturePassData>("CopyToTexture Pass",
			[=](CopyToTexturePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(render_target, RGLoadStoreAccessOp::Preserve_Preserve);
				data.texture_src = builder.ReadTexture(texture_src, ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CopyToTexturePassData const& data, RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

				switch (mode)
				{
				case BlendMode::None:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Copy));
					break;
				case BlendMode::AlphaBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Copy_AlphaBlend));
					break;
				case BlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Copy_AdditiveBlend));
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
				}

				GfxDescriptor dst = descriptor_allocator->Allocate();
				gfx->CopyDescriptors(1, dst, context.GetReadOnlyTexture(data.texture_src));

				cmd_list->SetRootConstant(1, dst.GetIndex(), 0);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleStrip);
				cmd_list->Draw(4);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

}
