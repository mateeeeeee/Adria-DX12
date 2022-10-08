#pragma once
#include <mutex>
#include "DynamicAllocation.h"
#include "../Utilities/LinearAllocator.h"

namespace adria
{
	class Buffer;
	class GraphicsDevice;
	
	class LinearDynamicAllocator
	{
	public:
		LinearDynamicAllocator(GraphicsDevice* gfx, SIZE_T max_size_in_bytes);
		~LinearDynamicAllocator();
		DynamicAllocation Allocate(SIZE_T size_in_bytes, SIZE_T alignment = 0);

		void Clear();

	private:
		LinearAllocator linear_allocator;
		std::mutex alloc_mutex;
		std::unique_ptr<Buffer> buffer;
		void* cpu_address;
	};


}