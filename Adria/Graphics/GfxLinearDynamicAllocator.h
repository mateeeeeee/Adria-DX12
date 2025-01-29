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
		static constexpr Uint32 PAGE_COUNT_HISTORY_SIZE = 8;
		struct GfxAllocationPage
		{
			std::unique_ptr<GfxBuffer> buffer;
			LinearAllocator linear_allocator;
			PVoid cpu_address;

			GfxAllocationPage(GfxDevice* gfx, Uint64 page_size);
			GfxAllocationPage(GfxAllocationPage&&);
			~GfxAllocationPage();
		};
	public:
		GfxLinearDynamicAllocator(GfxDevice* gfx, Uint64 page_size, Uint64 page_count = 1);
		~GfxLinearDynamicAllocator();
		GfxDynamicAllocation Allocate(Uint64 size_in_bytes, Uint64 alignment = 0);

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
		Uint64 const page_size;
		Uint64 current_page = 0;
		Uint64 used_page_count_history[PAGE_COUNT_HISTORY_SIZE];
	};
}