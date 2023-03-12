#pragma once
#include <mutex>
#include "DynamicAllocation.h"
#include "../Utilities/LinearAllocator.h"

namespace adria
{
	class GfxBuffer;
	class GfxDevice;
	
	class LinearDynamicAllocator
	{
	public:
		LinearDynamicAllocator(GfxDevice* gfx, SIZE_T max_size_in_bytes);
		~LinearDynamicAllocator();
		DynamicAllocation Allocate(SIZE_T size_in_bytes, SIZE_T alignment = 0);

		void Clear();

	private:
		LinearAllocator linear_allocator;
		std::mutex alloc_mutex;
		std::unique_ptr<GfxBuffer> buffer;
		void* cpu_address;
	};


}