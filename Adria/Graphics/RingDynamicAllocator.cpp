#include "RingDynamicAllocator.h"
#include "Buffer.h"


namespace adria
{
	RingDynamicAllocator::RingDynamicAllocator(GraphicsDevice* gfx, SIZE_T max_size_in_bytes)
		: ring_allocator(max_size_in_bytes)
	{
		BufferDesc desc{};
		desc.size = max_size_in_bytes;
		desc.resource_usage = EResourceUsage::Upload;

		buffer = std::make_unique<Buffer>(gfx, desc);
		ADRIA_ASSERT(buffer->IsMapped());
		cpu_address = buffer->GetMappedData();
	}

	RingDynamicAllocator::~RingDynamicAllocator() = default;

	DynamicAllocation RingDynamicAllocator::Allocate(SIZE_T size_in_bytes, SIZE_T alignment)
	{
		OffsetType offset = INVALID_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			offset = ring_allocator.Allocate(size_in_bytes, alignment);
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
			return DynamicAllocation{};
		}
	}
	void RingDynamicAllocator::FinishCurrentFrame(uint64 frame)
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		ring_allocator.FinishCurrentFrame(frame);
	}
	void RingDynamicAllocator::ReleaseCompletedFrames(uint64 completed_frame)
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		ring_allocator.ReleaseCompletedFrames(completed_frame);
	}
}