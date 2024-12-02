#pragma once
#include "GfxDescriptor.h"
#include "Utilities/Ref.h"

namespace adria
{
	class GfxDevice;

	enum class GfxDescriptorHeapType : Uint8
	{
		CBV_SRV_UAV = 0,
		Sampler,
		RTV,
		DSV,
		Count,
		Invalid
	};

	inline constexpr D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12HeapType(GfxDescriptorHeapType type)
	{
		switch (type)
		{
		case GfxDescriptorHeapType::CBV_SRV_UAV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		case GfxDescriptorHeapType::Sampler:
			return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		case GfxDescriptorHeapType::RTV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case GfxDescriptorHeapType::DSV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		}
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	}

	class GfxDescriptorAllocatorBase
	{
	public:
		GfxDescriptor GetHandle(Uint32 index = 0) const;
		ID3D12DescriptorHeap* GetHeap() const { return heap.Get(); }
		
	protected:
		GfxDevice* gfx;
		Ref<ID3D12DescriptorHeap> heap = nullptr;
		GfxDescriptor head_descriptor;
		Uint32 descriptor_handle_size = 0;
		GfxDescriptorHeapType type = GfxDescriptorHeapType::Invalid;
		Uint32 descriptor_count = 0;
		Bool shader_visible = false;

	protected:
		GfxDescriptorAllocatorBase(GfxDevice* gfx, GfxDescriptorHeapType type, Uint32 descriptor_count, Bool shader_visible);
		void CreateHeap();
	};
}