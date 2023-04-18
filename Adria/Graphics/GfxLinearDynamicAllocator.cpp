#include "GfxLinearDynamicAllocator.h"
#include "GfxBuffer.h"

namespace adria
{
	GfxLinearDynamicAllocator::GfxLinearDynamicAllocator(GfxDevice* gfx, size_t max_size_in_bytes)
		: linear_allocator(max_size_in_bytes)
	{
		GfxBufferDesc desc{};
		desc.size = max_size_in_bytes;
		desc.resource_usage = GfxResourceUsage::Upload;
		desc.bind_flags = GfxBindFlag::ShaderResource;

		buffer = std::make_unique<GfxBuffer>(gfx, desc);
		ADRIA_ASSERT(buffer->IsMapped());
		cpu_address = buffer->GetMappedData();
	}
	GfxLinearDynamicAllocator::~GfxLinearDynamicAllocator() = default;

	GfxDynamicAllocation GfxLinearDynamicAllocator::Allocate(size_t size_in_bytes, size_t alignment)
	{
		OffsetType offset = INVALID_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_mutex);
			offset = linear_allocator.Allocate(size_in_bytes, alignment);
		}

		if (offset != INVALID_OFFSET)
		{
			GfxDynamicAllocation allocation{};
			allocation.buffer = buffer.get();
			allocation.cpu_address = reinterpret_cast<uint8*>(cpu_address) + offset;
			allocation.gpu_address = buffer->GetGpuAddress() + offset;
			allocation.offset = offset;
			allocation.size = size_in_bytes;

			return allocation;
		}
		else
		{
			ADRIA_ASSERT(false && "Not enough memory in Dynamic Allocator. Increase the size to avoid this error!");
			return GfxDynamicAllocation{}; //return optional?
		}
	}
	void GfxLinearDynamicAllocator::Clear()
	{
		std::lock_guard<std::mutex> guard(alloc_mutex);
		linear_allocator.Clear();
	}
}