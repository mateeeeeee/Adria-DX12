#include "PickingPass.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "GlobalBlackboardData.h"
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
			RGBufferReadWriteId pick_buffer_uav;
			RGTextureReadOnlyId depth_srv;
			RGTextureReadOnlyId normal_srv;
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

				data.pick_buffer_uav = builder.WriteBuffer(RG_RES_NAME(PickBuffer));
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				data.normal_srv = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
			},
			[=](PickingPassDispatchData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				UINT backbuffer_index = gfx->BackbufferIndex();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Picker));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Picker));

				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyTexture(data.depth_srv), context.GetReadOnlyTexture(data.normal_srv) };
				uint32 src_range_sizes[] = { 1,1 };

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				uint32 dst_range_sizes[] = { (uint32)_countof(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, dst_descriptor);

				descriptor_index = descriptor_allocator->Allocate();
				dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, context.GetReadWriteBuffer(data.pick_buffer_uav), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, dst_descriptor);

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

