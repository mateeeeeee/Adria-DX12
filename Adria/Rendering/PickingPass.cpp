#include "PickingPass.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "BlackboardData.h"
#include "../Logging/Logger.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	PickingPass::PickingPass(GraphicsDevice* gfx, uint32 width, uint32 height) : gfx(gfx),
		width(width), height(height)
	{
		for (size_t i = 0; i < gfx->BackbufferCount(); ++i)
		{
			read_picking_buffers.emplace_back(std::make_unique<Buffer>(gfx, ReadBackBufferDesc(sizeof(PickingData))));
		}
	}

	void PickingPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void PickingPass::AddPass(RenderGraph& rg)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct PickingPassDispatchData
		{
			RGBufferReadWriteId pick_buffer;
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
		};

		rg.AddPass<PickingPassDispatchData>("Picking Pass Dispatch",
			[=](PickingPassDispatchData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc pick_buffer_desc{};
				pick_buffer_desc.resource_usage = EResourceUsage::Default;
				pick_buffer_desc.misc_flags = EBufferMiscFlag::BufferStructured;
				pick_buffer_desc.stride = sizeof(PickingData);
				pick_buffer_desc.size = pick_buffer_desc.stride;
				builder.DeclareBuffer(RG_RES_NAME(PickBuffer), pick_buffer_desc);

				data.pick_buffer = builder.WriteBuffer(RG_RES_NAME(PickBuffer));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				data.normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
			},
			[=](PickingPassDispatchData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				
				uint32 i = (uint32)descriptor_allocator->AllocateRange(3);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.normal), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadWriteBuffer(data.pick_buffer), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				struct PickingConstants
				{
					uint32 depth_idx;
					uint32 normal_idx;
					uint32 buffer_idx;
				} constants =
				{
					.depth_idx = i, .normal_idx = i + 1, .buffer_idx = i + 2
				};

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Picking));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.new_frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 3, &constants, 0);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
				cmd_list->Dispatch(1, 1, 1);
			}, ERGPassType::Compute, ERGPassFlags::ForceNoCull);

		struct PickingPassCopyData
		{
			RGBufferCopySrcId src;
		};

		rg.AddPass<PickingPassCopyData>("Picking Pass Copy",
			[=](PickingPassCopyData& data, RenderGraphBuilder& builder)
			{
				data.src = builder.ReadCopySrcBuffer(RG_RES_NAME(PickBuffer));
			},
			[=, backbuffer_index = gfx->BackbufferIndex()](PickingPassCopyData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Buffer const& buffer = context.GetCopySrcBuffer(data.src);
				cmd_list->CopyResource(read_picking_buffers[backbuffer_index]->GetNative(), buffer.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::ForceNoCull);
	}

	PickingData PickingPass::GetPickingData() const
	{
		UINT backbuffer_index = gfx->BackbufferIndex();
		PickingData const* data = read_picking_buffers[backbuffer_index]->GetMappedData<PickingData>();
		PickingData picking_data = *data;
		return picking_data;
	}

}

