#pragma once
#include "GfxDynamicAllocation.h"
#include "Utilities/RingAllocator.h"
#include <mutex>

namespace adria
{
	class GfxBuffer;
	class GfxDevice;

	class GfxRingDynamicAllocator
	{
	public:
		GfxRingDynamicAllocator(GfxDevice* gfx, size_t max_size_in_bytes);
		~GfxRingDynamicAllocator();
		GfxDynamicAllocation Allocate(size_t size_in_bytes, size_t alignment);
		template<typename T>
		GfxDynamicAllocation AllocateCBuffer()
		{
			return Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}
		void FinishCurrentFrame(uint64 frame);
		void ReleaseCompletedFrames(uint64 completed_frame);

	private:
		RingAllocator ring_allocator;
		std::mutex alloc_mutex;
		std::unique_ptr<GfxBuffer> buffer;
		void* cpu_address;
	};
}