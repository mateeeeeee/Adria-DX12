#include "AddTexturesPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	AddTexturesPass::AddTexturesPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void AddTexturesPass::AddPass(RenderGraph& rendergraph, RGResourceName render_target, RGResourceName texture1, RGResourceName texture2, BlendMode mode /*= EBlendMode::None*/)
	{
		struct CopyToTexturePassData
		{
			RGTextureReadOnlyId texture_src1;
			RGTextureReadOnlyId texture_src2;
		};

		rendergraph.AddPass<CopyToTexturePassData>("AddTextures Pass",
			[=](CopyToTexturePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(render_target, RGLoadStoreAccessOp::Preserve_Preserve);
				data.texture_src1 = builder.ReadTexture(texture1, ReadAccess_PixelShader);
				data.texture_src2 = builder.ReadTexture(texture2, ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CopyToTexturePassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				
				switch (mode)
				{
				case BlendMode::None:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Add));
					break;
				case BlendMode::AlphaBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Add_AlphaBlend));
					break;
				case BlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Add_AdditiveBlend));
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
				}

				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyTexture(data.texture_src1),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), context.GetReadOnlyTexture(data.texture_src2),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRoot32BitConstant(1, i, 0);
				cmd_list->SetGraphicsRoot32BitConstant(1, i + 1, 1);
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void AddTexturesPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

