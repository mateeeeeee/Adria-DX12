#pragma once
#include <list>
#include "GfxDescriptorAllocatorBase.h"

namespace adria
{
	enum class GfxDescriptorHeapType : uint8
	{
		CBV_SRV_UAV = 0,
		Sampler,
		RTV,
		DSV,
		Count,
		Invalid
	};

	struct GfxDescriptorAllocatorDesc
	{
		GfxDescriptorHeapType type = GfxDescriptorHeapType::Invalid;
		uint32 descriptor_count = 0;
		bool shader_visible = false;
	};

	class GfxDescriptorAllocator : public GfxDescriptorAllocatorBase
	{
		struct GfxDescriptorRange
		{
			GfxDescriptor begin;
			GfxDescriptor end;
		};
	public:

		GfxDescriptorAllocator(GfxDevice* gfx_device, GfxDescriptorAllocatorDesc const& desc);
		~GfxDescriptorAllocator();

		[[nodiscard]] GfxDescriptor AllocateDescriptor();
		void FreeDescriptor(GfxDescriptor handle);

	private:
		GfxDescriptor tail_descriptor;
		std::list<GfxDescriptorRange> free_descriptor_ranges;
	};
}