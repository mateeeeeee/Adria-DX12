#include "LinearDynamicAllocator.h"

#include "../Logging/Logger.h"

namespace adria
{
	LinearDynamicAllocator::LinearDynamicAllocator(ID3D12Device* device, SIZE_T max_size_in_bytes)
		: linear_allocator(max_size_in_bytes)
	{
		auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(max_size_in_bytes);
		BREAK_IF_FAILED(device->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&buffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer)));

		CD3DX12_RANGE read_range(0, 0);
		BREAK_IF_FAILED(buffer->Map(0, &read_range, reinterpret_cast<void**>(&cpu_address)));
		gpu_address = buffer->GetGPUVirtualAddress();
	}
	DynamicAllocation LinearDynamicAllocator::Allocate(SIZE_T size_in_bytes, SIZE_T alignment)
	{
		OffsetType offset = INVALID_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			offset = linear_allocator.Allocate(size_in_bytes, alignment);
		}
		
		if (offset != INVALID_OFFSET)
		{
			DynamicAllocation allocation{}; 
			allocation.buffer = buffer.Get();
			allocation.cpu_address = reinterpret_cast<uint8*>(cpu_address) + offset;
			allocation.gpu_address = gpu_address + offset;
			allocation.offset = offset;
			allocation.size = size_in_bytes;

			return allocation;
		}
		else
		{
			ADRIA_ASSERT(false && "Not enough memory in Dynamic Allocator. Increase the size to avoid this error!");
			return DynamicAllocation{}; //return optional?
		}
	}
	void LinearDynamicAllocator::Clear()
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		linear_allocator.Clear();
	}

	D3D12_GPU_VIRTUAL_ADDRESS LinearDynamicAllocator::GPUAddress() const
	{
		return gpu_address;
	}

}