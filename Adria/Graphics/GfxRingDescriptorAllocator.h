#pragma once
#include <mutex>
#include "GfxDescriptorAllocatorBase.h"
#include "Utilities/RingAllocator.h"

namespace adria
{
	template<bool UseMutex>
	class GfxRingDescriptorAllocator : public GfxDescriptorAllocatorBase
	{
		using Mutex = std::conditional_t<UseMutex, std::mutex, DummyMutex>;
	public:
		GfxRingDescriptorAllocator(GfxDevice* gfx, Uint32 count, Uint32 reserve = 0)
			: GfxDescriptorAllocatorBase(gfx, GfxDescriptorHeapType::CBV_SRV_UAV, count, true),
			ring_allocator(count, reserve)
		{}

		~GfxRingDescriptorAllocator() = default;

		ADRIA_NODISCARD GfxDescriptor Allocate(Uint32 count = 1)
		{
			OffsetType start = INVALID_OFFSET;
			{
				std::lock_guard guard(alloc_mutex);
				start = ring_allocator.Allocate(count);
			}
			ADRIA_ASSERT(start != INVALID_OFFSET && "Don't have enough space");
			return GetHandle((Uint32)start);
		}

		void FinishCurrentFrame(Uint64 frame)
		{
			std::lock_guard guard(alloc_mutex);
			ring_allocator.FinishCurrentFrame(frame);
		}
		void ReleaseCompletedFrames(Uint64 completed_frame)
		{
			std::lock_guard guard(alloc_mutex);
			ring_allocator.ReleaseCompletedFrames(completed_frame);
		}

	private:
		mutable Mutex alloc_mutex;
		RingAllocator ring_allocator;
	};
}