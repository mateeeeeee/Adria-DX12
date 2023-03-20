#pragma once
#include "GfxDescriptor.h"
#include "../Utilities/AutoRefCountPtr.h"

namespace adria
{
	class GfxDevice;

	class GfxDescriptorAllocatorBase
	{
	public:

		GfxDescriptor GetHandle(size_t index = 0) const;

	protected:
		GfxDevice* gfx;
		ArcPtr<ID3D12DescriptorHeap> heap = nullptr;
		GfxDescriptor head_descriptor;
		uint32 descriptor_handle_size = 0;
		GfxDescriptorHeapType type = GfxDescriptorHeapType::Invalid;
		uint32 descriptor_count = 0;
		bool shader_visible = false;

	protected:

		GfxDescriptorAllocatorBase(GfxDevice* gfx, GfxDescriptorHeapType type, uint32 descriptor_count, bool shader_visible);
		void CreateHeap();
	};
}