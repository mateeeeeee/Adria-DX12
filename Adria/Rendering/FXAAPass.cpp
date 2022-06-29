#include "FXAAPass.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	FXAAPass::FXAAPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void FXAAPass::AddPass(RenderGraph& rg, RGResourceName input, bool render_to_backbuffer)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		ERGPassFlags flags = render_to_backbuffer ? ERGPassFlags::ForceNoCull | ERGPassFlags::SkipAutoRenderPass : ERGPassFlags::None;

		struct FXAAPassData
		{
			RGRenderTargetId	render_target;
			RGTextureReadOnlyId ldr_src;
		};

		rg.AddPass<FXAAPassData>("FXAA Pass",
			[=](FXAAPassData& data, RenderGraphBuilder& builder)
			{
				data.ldr_src = builder.ReadTexture(input, ReadAccess_PixelShader);
				ADRIA_ASSERT(!render_to_backbuffer && builder.IsTextureDeclared(RG_RES_NAME(FinalTexture)));
				if (!render_to_backbuffer) data.render_target = builder.WriteRenderTarget(RG_RES_NAME(FinalTexture), ERGLoadStoreAccessOp::Discard_Preserve);
				else data.render_target.Invalidate();
				builder.SetViewport(width, height);
			},
			[=](FXAAPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				if (!data.render_target.IsValid())
				{
					D3D12_VIEWPORT vp{};
					vp.Width = (float32)width;
					vp.Height = (float32)height;
					vp.MinDepth = 0.0f;
					vp.MaxDepth = 1.0f;
					vp.TopLeftX = 0;
					vp.TopLeftY = 0;
					cmd_list->RSSetViewports(1, &vp);
					D3D12_RECT rect{};
					rect.bottom = (int64)height;
					rect.left = 0;
					rect.right = (int64)width;
					rect.top = 0;
					cmd_list->RSSetScissorRects(1, &rect);
					gfx->SetBackbuffer(cmd_list);
				}

				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::FXAA));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::FXAA));
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), context.GetReadOnlyTexture(data.ldr_src),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, ERGPassType::Graphics, flags);

	}

	void FXAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}
