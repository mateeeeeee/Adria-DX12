#include "CopyToTexturePass.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	void CopyToTexturePass::AddPass(RenderGraph& rendergraph, RGResourceName render_target, RGResourceName texture_src, EBlendMode mode /*= EBlendMode::None*/)
	{
		struct CopyToTexturePassData
		{
			RGTextureReadOnlyId texture_src;
		};

		rendergraph.AddPass<CopyToTexturePassData>("CopyToTexture Pass",
			[=](CopyToTexturePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(render_target, ERGLoadStoreAccessOp::Preserve_Preserve);
				data.texture_src = builder.ReadTexture(texture_src, ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CopyToTexturePassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Copy));

				switch (mode)
				{
				case EBlendMode::None:
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Copy));
					break;
				case EBlendMode::AlphaBlend:
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Copy_AlphaBlend));
					break;
				case EBlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Copy_AdditiveBlend));
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
				}

				OffsetType descriptor_index = descriptor_allocator->Allocate();

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), resources.GetDescriptor(data.texture_src),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);

	}

}
