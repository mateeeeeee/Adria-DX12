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
		GfxRingDynamicAllocator(GfxDevice* gfx, Uint64 max_size_in_bytes);
		~GfxRingDynamicAllocator();
		GfxDynamicAllocation Allocate(Uint64 size_in_bytes, Uint64 alignment);
		template<typename T>
		GfxDynamicAllocation AllocateCBuffer()
		{
			return Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}
		void FinishCurrentFrame(Uint64 frame);
		void ReleaseCompletedFrames(Uint64 completed_frame);

	private:
		RingAllocator ring_allocator;
		std::mutex alloc_mutex;
		std::unique_ptr<GfxBuffer> buffer;
		void* cpu_address;
	};
}