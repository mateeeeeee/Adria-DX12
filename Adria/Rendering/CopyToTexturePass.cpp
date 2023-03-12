#include "CopyToTexturePass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"

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
			[=](CopyToTexturePassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				
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

				uint32 i = (uint32)descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyTexture(data.texture_src),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRoot32BitConstant(1, i, 0);
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

}
