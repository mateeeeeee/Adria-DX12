#include "GfxRingDynamicAllocator.h"
#include "GfxBuffer.h"
#include "GfxDevice.h"

namespace adria
{
	GfxRingDynamicAllocator::GfxRingDynamicAllocator(GfxDevice* gfx, uint64 max_size_in_bytes)
		: ring_allocator(max_size_in_bytes)
	{
		GfxBufferDesc desc{};
		desc.size = max_size_in_bytes;
		desc.resource_usage = GfxResourceUsage::Upload;
		desc.bind_flags = GfxBindFlag::ShaderResource;

		buffer = gfx->CreateBuffer(desc);
		ADRIA_ASSERT(buffer->IsMapped());
		cpu_address = buffer->GetMappedData();
	}

	GfxRingDynamicAllocator::~GfxRingDynamicAllocator() = default;

	GfxDynamicAllocation GfxRingDynamicAllocator::Allocate(uint64 size_in_bytes, uint64 alignment)
	{
		OffsetType offset = INVALID_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			offset = ring_allocator.Allocate(size_in_bytes, alignment);
		}

		if (offset != INVALID_OFFSET)
		{
			GfxDynamicAllocation allocation{};
			allocation.buffer = buffer.get();
			allocation.cpu_address = reinterpret_cast<uint8*>(cpu_address) + offset;
			allocation.gpu_address = buffer->GetGpuAddress() + offset;
			allocation.offset = offset;
			allocation.size = size_in_bytes;
			return allocation;
		}
		else
		{
			ADRIA_ASSERT(false && "Not enough memory in Dynamic Allocator. Increase the size to avoid this error!");
			return GfxDynamicAllocation{};
		}
	}
	void GfxRingDynamicAllocator::FinishCurrentFrame(uint64 frame)
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		ring_allocator.FinishCurrentFrame(frame);
	}
	void GfxRingDynamicAllocator::ReleaseCompletedFrames(uint64 completed_frame)
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		ring_allocator.ReleaseCompletedFrames(completed_frame);
	}
}