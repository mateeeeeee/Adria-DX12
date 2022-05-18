#pragma once
#include <memory>
#include <DirectXMath.h>
#include "RootSigPSOManager.h"
#include "../Graphics/Buffer.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/ResourceBarrierBatch.h"
#include "../Graphics/GraphicsDeviceDX12.h" 
#include "../Logging/Logger.h"


namespace adria
{
	struct PickingData
	{
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT4 normal;
	};

	class Picker
	{
		friend class Renderer;
	private:
		Picker(GraphicsDevice* gfx) : gfx(gfx), 
			write_picking_buffer(gfx, StructuredBufferDesc<PickingData>(1)),
			read_picking_buffer{ {gfx, ReadBackBufferDesc(sizeof(PickingData))}, 
								 {gfx, ReadBackBufferDesc(sizeof(PickingData))}, 
								 {gfx, ReadBackBufferDesc(sizeof(PickingData))}}
		{
		}

		void CreateView(D3D12_CPU_DESCRIPTOR_HANDLE uav_handle)
		{
			BufferViewDesc uav_desc{};
			uav_desc.view_type = EResourceViewType::UAV;
			write_picking_buffer.CreateView(uav_desc, uav_handle);
		}

		void Pick(ID3D12GraphicsCommandList4* cmd_list, 
			D3D12_CPU_DESCRIPTOR_HANDLE depth_handle, 
			D3D12_CPU_DESCRIPTOR_HANDLE normal_handle,
			D3D12_GPU_VIRTUAL_ADDRESS frame_cbuffer_gpu_address)
		{
			ID3D12Device* device = gfx->GetDevice();
			RingDescriptorAllocator* descriptor_allocator = gfx->GetDescriptorAllocator();
			UINT backbuffer_index = gfx->BackbufferIndex();

			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Picker));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Picker));

			cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer_gpu_address);
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { depth_handle, normal_handle };
			uint32 src_range_sizes[] = { 1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
			auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)_countof(cpu_handles) };
			device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, dst_descriptor);

			descriptor_index = descriptor_allocator->Allocate();
			dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			device->CopyDescriptorsSimple(1, dst_descriptor, write_picking_buffer.GetView(EResourceViewType::UAV), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, dst_descriptor);

			ResourceBarrierBatch barrier_batch{};
			barrier_batch.AddTransition(write_picking_buffer.GetNative(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			barrier_batch.Submit(cmd_list);

			cmd_list->Dispatch(1, 1, 1);

			barrier_batch.ReverseTransitions();
			barrier_batch.Submit(cmd_list);
			cmd_list->CopyResource(read_picking_buffer[backbuffer_index].GetNative(), write_picking_buffer.GetNative());
		}

		PickingData GetPickingData() const
		{
			UINT backbuffer_index = gfx->BackbufferIndex();
			PickingData const* data = read_picking_buffer[backbuffer_index].GetMappedData<PickingData>();
			PickingData picking_data = *data;
			//read_picking_buffer[backbuffer_index].Unmap();
			return picking_data;
		}

	private:

		GraphicsDevice* gfx;
		Buffer write_picking_buffer;
		Buffer read_picking_buffer[GraphicsDevice::BackbufferCount()];
		ShaderBlob picker_blob;
	};
}