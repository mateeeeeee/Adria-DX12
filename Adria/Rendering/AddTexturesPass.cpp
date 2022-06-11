#include "AddTexturesPass.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	AddTexturesPass::AddTexturesPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void AddTexturesPass::AddPass(RenderGraph& rendergraph, RGResourceName render_target, RGResourceName texture1, RGResourceName texture2, EBlendMode mode /*= EBlendMode::None*/)
	{
		struct CopyToTexturePassData
		{
			RGTextureReadOnlyId texture_src1;
			RGTextureReadOnlyId texture_src2;
		};

		rendergraph.AddPass<CopyToTexturePassData>("AddTextures Pass",
			[=](CopyToTexturePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(render_target, ERGLoadStoreAccessOp::Preserve_Preserve);
				data.texture_src1 = builder.ReadTexture(texture1, ReadAccess_PixelShader);
				data.texture_src2 = builder.ReadTexture(texture2, ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CopyToTexturePassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Add));

				switch (mode)
				{
				case EBlendMode::None:
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Add));
					break;
				case EBlendMode::AlphaBlend:
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Add_AlphaBlend));
					break;
				case EBlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Add_AdditiveBlend));
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
				}

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), resources.GetDescriptor(data.texture_src1),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), resources.GetDescriptor(data.texture_src2),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

	void AddTexturesPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

