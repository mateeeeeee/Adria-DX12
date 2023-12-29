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
		static constexpr uint32 PAGE_COUNT_HISTORY_SIZE = 8;
		struct GfxAllocationPage
		{
			std::unique_ptr<GfxBuffer> buffer;
			LinearAllocator linear_allocator;
			void* cpu_address;

			GfxAllocationPage(GfxDevice* gfx, uint64 page_size);
			GfxAllocationPage(GfxAllocationPage&&);
			~GfxAllocationPage();
		};
	public:
		GfxLinearDynamicAllocator(GfxDevice* gfx, uint64 page_size, uint64 page_count = 1);
		~GfxLinearDynamicAllocator();
		GfxDynamicAllocation Allocate(uint64 size_in_bytes, uint64 alignment = 0);

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
		uint64 const page_size;
		uint64 current_page = 0;
		uint64 used_page_count_history[PAGE_COUNT_HISTORY_SIZE];
	};
}