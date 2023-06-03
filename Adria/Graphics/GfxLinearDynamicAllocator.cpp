#include "GfxLinearDynamicAllocator.h"
#include "GfxBuffer.h"

namespace adria
{
	GfxLinearDynamicAllocator::GfxLinearDynamicAllocator(GfxDevice* gfx, size_t page_size, size_t page_count)
		: gfx(gfx), page_size(page_size)
	{
		alloc_pages.reserve(page_count);
		while (alloc_pages.size() < page_count) alloc_pages.emplace_back(gfx, page_size);
	}
	GfxLinearDynamicAllocator::~GfxLinearDynamicAllocator() = default;

	GfxDynamicAllocation GfxLinearDynamicAllocator::Allocate(size_t size_in_bytes, size_t alignment)
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
			GfxAllocationPage& last_page = current_page < alloc_pages.size() ? alloc_pages[current_page] : alloc_pages.emplace_back(gfx, page_size);
			return Allocate(size_in_bytes, alignment);
		}
	}
	void GfxLinearDynamicAllocator::Clear()
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		for (auto& page : alloc_pages) page.linear_allocator.Clear();
		current_page = 0;
	}

	GfxLinearDynamicAllocator::GfxAllocationPage::GfxAllocationPage(GfxDevice* gfx, size_t page_size) : linear_allocator(page_size)
	{
		GfxBufferDesc desc{};
		desc.size = page_size;
		desc.resource_usage = GfxResourceUsage::Upload;
		desc.bind_flags = GfxBindFlag::ShaderResource;

		buffer = std::make_unique<GfxBuffer>(gfx, desc);
		ADRIA_ASSERT(buffer->IsMapped());
		cpu_address = buffer->GetMappedData();
	}
	GfxLinearDynamicAllocator::GfxAllocationPage::GfxAllocationPage(GfxAllocationPage&&) = default;
	GfxLinearDynamicAllocator::GfxAllocationPage::~GfxAllocationPage() = default;

}