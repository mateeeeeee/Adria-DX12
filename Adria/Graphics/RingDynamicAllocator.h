#pragma once
#include "DynamicAllocation.h"
#include "../Utilities/RingAllocator.h"
#include <mutex>

namespace adria
{
	class Buffer;
	class GraphicsDevice;

	class RingDynamicAllocator
	{
	public:
		RingDynamicAllocator(GraphicsDevice* gfx, SIZE_T max_size_in_bytes);
		~RingDynamicAllocator();
		DynamicAllocation Allocate(SIZE_T size_in_bytes, SIZE_T alignment);

		void FinishCurrentFrame(uint64 frame);

		void ReleaseCompletedFrames(uint64 completed_frame);

	private:
		RingAllocator ring_allocator;
		std::mutex alloc_mutex;
		std::unique_ptr<Buffer> buffer;
		void* cpu_address;
	};
}