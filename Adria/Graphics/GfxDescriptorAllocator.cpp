#include "GfxDescriptorAllocator.h"

namespace adria
{
	GfxDescriptorAllocator::GfxDescriptorAllocator(GfxDevice* gfx, GfxDescriptorAllocatorDesc const& desc)
		: GfxDescriptorAllocatorBase(gfx, desc.type, desc.descriptor_count, desc.shader_visible),
		tail_descriptor(head_descriptor)
	{
		tail_descriptor.Increment(descriptor_handle_size ,desc.descriptor_count - 1);
		free_descriptor_ranges.emplace_back(head_descriptor, tail_descriptor);
	}

	GfxDescriptorAllocator::~GfxDescriptorAllocator() = default;

	GfxDescriptor GfxDescriptorAllocator::AllocateDescriptor()
	{
		GfxDescriptorRange& range = free_descriptor_ranges.front();
		GfxDescriptor handle = range.begin;
		range.begin.Increment(descriptor_handle_size);
		if (range.begin == range.end) free_descriptor_ranges.pop_front();
		return handle;
	}

	void GfxDescriptorAllocator::FreeDescriptor(GfxDescriptor handle)
	{
		GfxDescriptor incremented_handle = handle;
		incremented_handle.Increment(descriptor_handle_size);

		GfxDescriptorRange rng{
			.begin = handle,
			.end = incremented_handle
		};

		bool found = false;
		for (auto range = std::begin(free_descriptor_ranges);
			range != std::end(free_descriptor_ranges) && found == false; ++range)
		{
			if (range->begin == incremented_handle)
			{
				range->begin = handle;
				found = true;
			}
			else if (range->end == handle)
			{
				range->end.Increment(descriptor_handle_size);
				found = true;
			}
			else if (range->begin.GetIndex() > handle.GetIndex())
			{
				free_descriptor_ranges.insert(range, rng);
				found = true;
			}
		}
		if (!found) free_descriptor_ranges.push_back(rng);
	}

}

