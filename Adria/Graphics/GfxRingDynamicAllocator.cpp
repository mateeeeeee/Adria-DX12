#include "GfxRingDynamicAllocator.h"
#include "GfxBuffer.h"
#include "GfxDevice.h"

namespace adria
{
	GfxRingDynamicAllocator::GfxRingDynamicAllocator(GfxDevice* gfx, Uint64 max_size_in_bytes)
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

	GfxDynamicAllocation GfxRingDynamicAllocator::Allocate(Uint64 size_in_bytes, Uint64 alignment)
	{
		Uint64 offset = INVALID_ALLOC_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			offset = ring_allocator.Allocate(size_in_bytes, alignment);
		}

		if (offset != INVALID_ALLOC_OFFSET)
		{
			GfxDynamicAllocation allocation{};
			allocation.buffer = buffer.get();
			allocation.cpu_address = reinterpret_cast<Uint8*>(cpu_address) + offset;
			allocation.gpu_address = buffer->GetGpuAddress() + offset;
			allocation.offset = offset;
			allocation.size = size_in_bytes;
			return allocation;
		}
		else
		{
			ADRIA_ASSERT_MSG(false, "Not enough memory in Dynamic Allocator. Increase the size to avoid this error!");
			return GfxDynamicAllocation{};
		}
	}
	void GfxRingDynamicAllocator::FinishCurrentFrame(Uint64 frame)
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		ring_allocator.FinishCurrentFrame(frame);
	}
	void GfxRingDynamicAllocator::ReleaseCompletedFrames(Uint64 completed_frame)
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		ring_allocator.ReleaseCompletedFrames(completed_frame);
	}
}