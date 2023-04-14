#pragma once
#include <mutex>
#include "GfxDynamicAllocation.h"
#include "Utilities/LinearAllocator.h"

namespace adria
{
	class GfxBuffer;
	class GfxDevice;
	
	class GfxLinearDynamicAllocator
	{
	public:
		GfxLinearDynamicAllocator(GfxDevice* gfx, size_t max_size_in_bytes);
		~GfxLinearDynamicAllocator();
		GfxDynamicAllocation Allocate(size_t size_in_bytes, size_t alignment = 0);
		template<typename T>
		GfxDynamicAllocation AllocateCBuffer()
		{
			return Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}
		void Clear();

	private:
		LinearAllocator linear_allocator;
		std::mutex alloc_mutex;
		std::unique_ptr<GfxBuffer> buffer;
		void* cpu_address;
	};
}