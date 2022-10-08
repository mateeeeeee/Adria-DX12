#include "LinearDynamicAllocator.h"
#include "Buffer.h"

namespace adria
{
	LinearDynamicAllocator::LinearDynamicAllocator(GraphicsDevice* gfx, SIZE_T max_size_in_bytes)
		: linear_allocator(max_size_in_bytes)
	{
		BufferDesc desc{};
		desc.size = max_size_in_bytes;
		desc.resource_usage = EResourceUsage::Upload;

		buffer = std::make_unique<Buffer>(gfx, desc);
		ADRIA_ASSERT(buffer->IsMapped());
		cpu_address = buffer->GetMappedData();
	}
	LinearDynamicAllocator::~LinearDynamicAllocator() = default;

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
			allocation.buffer = buffer->GetNative();
			allocation.cpu_address = reinterpret_cast<uint8*>(cpu_address) + offset;
			allocation.gpu_address = buffer->GetGPUAddress() + offset;
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
}