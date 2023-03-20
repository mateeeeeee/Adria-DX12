#pragma once
#include <mutex>
#include "../Utilities/RingAllocator.h"
#include "GfxDescriptorAllocatorBase.h"

namespace adria
{
	template<bool UseMutex>
	class GfxRingDescriptorAllocator : public GfxDescriptorAllocatorBase
	{
		using Mutex = std::conditional_t<UseMutex, std::mutex, DummyMutex>;
	public:
		GfxRingDescriptorAllocator(GfxDevice* gfx, uint32 count, uint32 reserve = 0)
			: GfxDescriptorAllocatorBase(gfx, GfxDescriptorHeapType::CBV_SRV_UAV, count, true),
			ring_allocator(count, reserve)
		{}

		~GfxRingDescriptorAllocator() = default;

		[[nodiscard]] GfxDescriptor Allocate(uint32 count = 1)
		{
			OffsetType start = INVALID_OFFSET;
			{
				std::lock_guard<std::mutex> guard(alloc_lock);
				start = ring_allocator.Allocate(range_size);
			}
			ADRIA_ASSERT(start != INVALID_OFFSET && "Don't have enough space");
			return GetHandle(start);
		}

		void FinishCurrentFrame(uint64 frame)
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			ring_allocator.FinishCurrentFrame(frame);
		}
		void ReleaseCompletedFrames(uint64 completed_frame)
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			ring_allocator.ReleaseCompletedFrames(completed_frame);
		}

	private:
		Mutex alloc_mutex;
		RingAllocator ring_allocator;
	};
}