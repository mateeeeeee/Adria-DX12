#include "ToneMapPass.h"
#include "GlobalBlackboardData.h"
#include "PipelineState.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	ToneMapPass::ToneMapPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void ToneMapPass::AddPass(RenderGraph& rg, RGResourceName hdr_src, bool render_to_backbuffer)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		ERGPassFlags flags = render_to_backbuffer ? ERGPassFlags::ForceNoCull | ERGPassFlags::SkipAutoRenderPass : ERGPassFlags::None;

		struct ToneMapPassData
		{
			RGRenderTargetId	target;
			RGTextureReadOnlyId hdr_srv;
		};

		rg.AddPass<ToneMapPassData>("ToneMap Pass",
			[=](ToneMapPassData& data, RenderGraphBuilder& builder)
			{
				data.hdr_srv = builder.ReadTexture(hdr_src, ReadAccess_PixelShader);
				if (!render_to_backbuffer)
				{
					ADRIA_ASSERT(builder.IsTextureDeclared(RG_RES_NAME(FinalTexture)));
					data.target = builder.WriteRenderTarget(RG_RES_NAME(FinalTexture), ERGLoadStoreAccessOp::Discard_Preserve);
				}
				else data.target = RGRenderTargetId();
				builder.SetViewport(width, height);
			},
			[=](ToneMapPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				if (!data.target.IsValid())
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

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ToneMap));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineState::ToneMap));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = context.GetReadOnlyTexture(data.hdr_srv);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			}, ERGPassType::Graphics, flags);
	}

	void ToneMapPass::AddPass(RenderGraph& rg, RGResourceName hdr_src, RGResourceName fxaa_input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		ERGPassFlags flags = ERGPassFlags::None;

		struct ToneMapPassData
		{
			RGRenderTargetId	target;
			RGTextureReadOnlyId hdr_srv;
		};

		rg.AddPass<ToneMapPassData>("ToneMap Pass",
			[=](ToneMapPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc fxaa_input_desc{};
				fxaa_input_desc.width = width;
				fxaa_input_desc.height = height;
				fxaa_input_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
				builder.DeclareTexture(fxaa_input, fxaa_input_desc);

				data.hdr_srv = builder.ReadTexture(hdr_src, ReadAccess_PixelShader);
				data.target = builder.WriteRenderTarget(fxaa_input, ERGLoadStoreAccessOp::Discard_Preserve);
				builder.SetViewport(width, height);
			},
			[=](ToneMapPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ToneMap));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineState::ToneMap));

				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.postprocess_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = context.GetReadOnlyTexture(data.hdr_srv);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
			});
	}

	void ToneMapPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}