#include "GfxDescriptorAllocatorBase.h"
#include "../Core/Macros.h"

namespace adria
{
	static constexpr D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12HeapType(GfxDescriptorHeapType type)
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
		ADRIA_UNREACHABLE();
	}


	GfxDescriptor GfxDescriptorAllocatorBase::GetHandle(size_t index /*= 0*/) const
	{
		ADRIA_ASSERT(heap != nullptr);
		ADRIA_ASSERT(index < descriptor_count);

		GfxDescriptor handle = head_descriptor;
		handle.Increment(descriptor_handle_size, index);
		return handle;
	}

	GfxDescriptorAllocatorBase::GfxDescriptorAllocatorBase(GfxDevice* gfx, GfxDescriptorHeapType type, uint32 descriptor_count, bool shader_visible) : gfx(gfx),
		type(type),
		descriptor_count(descriptor_count),
		shader_visible(shader_visible), head_descriptor{}
	{
		CreateHeap();
		head_descriptor.cpu = heap->GetCPUDescriptorHandleForHeapStart();
		if(shader_visible) head_descriptor.gpu = heap->GetGPUDescriptorHandleForHeapStart();
		head_descriptor.index = 0;
	}

	void GfxDescriptorAllocatorBase::CreateHeap()
	{
		ADRIA_ASSERT(count <= UINT32_MAX && "Too many descriptors");
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heap_desc.NumDescriptors = descriptor_count;
		heap_desc.Type = ToD3D12HeapType(type);
		BREAK_IF_FAILED(gfx->GetDevice()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())));
		descriptor_handle_size = gfx->GetDevice()->GetDescriptorHandleIncrementSize(heap_desc.Type);
	}

}