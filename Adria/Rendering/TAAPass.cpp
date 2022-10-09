#include "TAAPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/TextureManager.h"
#include "../Logging/Logger.h"
#include "../Editor/GUICommand.h"

namespace adria
{

	TAAPass::TAAPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName TAAPass::AddPass(RenderGraph& rg, RGResourceName input, RGResourceName history)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = input;

		struct TAAPassData
		{
			RGTextureReadOnlyId input_srv;
			RGTextureReadOnlyId history_srv;
			RGTextureReadOnlyId velocity_srv;
		};

		rg.AddPass<TAAPassData>("TAA Pass",
			[=](TAAPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc taa_desc{};
				taa_desc.width = width;
				taa_desc.height = height;
				taa_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.SetViewport(width, height);
				builder.DeclareTexture(RG_RES_NAME(TAAOutput), taa_desc);
				builder.WriteRenderTarget(RG_RES_NAME(TAAOutput), ERGLoadStoreAccessOp::Discard_Preserve);
				data.input_srv = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.history_srv = builder.ReadTexture(RG_RES_NAME(HistoryBuffer), ReadAccess_PixelShader);
				data.velocity_srv = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_PixelShader);
			},
			[=](TAAPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::TAA));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::TAA));

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), context.GetReadOnlyTexture(data.input_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), context.GetReadOnlyTexture(data.history_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 2), context.GetReadOnlyTexture(data.velocity_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);

		return RG_RES_NAME(TAAOutput);
	}

	void TAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}