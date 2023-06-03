#pragma once
#include <mutex>
#include "GfxDynamicAllocation.h"
#include "Utilities/LinearAllocator.h"

namespace adria
{
	class GfxBuffer;
	class GfxDevice;

	class GfxAllocationPage;
	
	class GfxLinearDynamicAllocator
	{
		struct GfxAllocationPage
		{
			std::unique_ptr<GfxBuffer> buffer;
			LinearAllocator linear_allocator;
			void* cpu_address;

			GfxAllocationPage(GfxDevice* gfx, size_t page_size);
			GfxAllocationPage(GfxAllocationPage&&);
			~GfxAllocationPage();
		};
	public:
		GfxLinearDynamicAllocator(GfxDevice* gfx, size_t page_size, size_t page_count = 1);
		~GfxLinearDynamicAllocator();
		GfxDynamicAllocation Allocate(size_t size_in_bytes, size_t alignment = 0);

		template<typename T>
		GfxDynamicAllocation AllocateCBuffer()
		{
			return Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}
		void Clear();

	private:
		GfxDevice* gfx;
		std::mutex alloc_mutex;
		std::vector<GfxAllocationPage> alloc_pages;
		size_t const page_size;
		size_t current_page = 0;
	};
}