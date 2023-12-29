#include "GfxLinearDynamicAllocator.h"
#include "GfxBuffer.h"
#include "GfxDevice.h"

namespace adria
{
	GfxLinearDynamicAllocator::GfxLinearDynamicAllocator(GfxDevice* gfx, uint64 page_size, uint64 page_count)
		: gfx(gfx), page_size(page_size)
	{
		alloc_pages.reserve(page_count);
		while (alloc_pages.size() < page_count) alloc_pages.emplace_back(gfx, page_size);
	}
	GfxLinearDynamicAllocator::~GfxLinearDynamicAllocator() = default;

	GfxDynamicAllocation GfxLinearDynamicAllocator::Allocate(uint64 size_in_bytes, uint64 alignment)
	{
		OffsetType offset = INVALID_OFFSET;
		GfxAllocationPage& last_page = alloc_pages[current_page];
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			offset = last_page.linear_allocator.Allocate(size_in_bytes, alignment);
		}

		if (offset != INVALID_OFFSET)
		{
			GfxDynamicAllocation allocation{};
			allocation.buffer = last_page.buffer.get();
			allocation.cpu_address = reinterpret_cast<uint8*>(last_page.cpu_address) + offset;
			allocation.gpu_address = last_page.buffer->GetGpuAddress() + offset;
			allocation.offset = offset;
			allocation.size = size_in_bytes;

			return allocation;
		}
		else
		{
			++current_page;
			GfxAllocationPage& last_page = current_page < alloc_pages.size() ? alloc_pages[current_page] : alloc_pages.emplace_back(gfx, std::max(size_in_bytes, page_size));
			return Allocate(size_in_bytes, alignment);
		}
	}
	void GfxLinearDynamicAllocator::Clear()
	{
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			for (auto& page : alloc_pages) page.linear_allocator.Clear();
		}
		
		uint32 i = gfx->GetFrameIndex() % PAGE_COUNT_HISTORY_SIZE;
		used_page_count_history[i] = current_page;
		uint64 max_used_page_count = 0;
		for (uint32 j = 0; j < PAGE_COUNT_HISTORY_SIZE; ++j) max_used_page_count = std::max(max_used_page_count, used_page_count_history[j]);
		if (max_used_page_count < alloc_pages.size())
		{
			while (alloc_pages.size() == max_used_page_count) 
				alloc_pages.pop_back();
		}
		current_page = 0;
	}

	GfxLinearDynamicAllocator::GfxAllocationPage::GfxAllocationPage(GfxDevice* gfx, uint64 page_size) : linear_allocator(page_size)
	{
		GfxBufferDesc desc{};
		desc.size = page_size;
		desc.resource_usage = GfxResourceUsage::Upload;
		desc.bind_flags = GfxBindFlag::ShaderResource;

		buffer = gfx->CreateBuffer(desc);
		ADRIA_ASSERT(buffer->IsMapped());
		cpu_address = buffer->GetMappedData();
	}
	GfxLinearDynamicAllocator::GfxAllocationPage::GfxAllocationPage(GfxAllocationPage&&) = default;

	GfxLinearDynamicAllocator::GfxAllocationPage::~GfxAllocationPage() = default;

}