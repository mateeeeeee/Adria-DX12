#include "GfxDescriptorAllocatorBase.h"
#include "GfxDevice.h"

namespace adria
{
	GfxDescriptor GfxDescriptorAllocatorBase::GetHandle(uint32 index /*= 0*/) const
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
		ADRIA_ASSERT(descriptor_count <= UINT32_MAX && "Too many descriptors");
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heap_desc.NumDescriptors = descriptor_count;
		heap_desc.Type = ToD3D12HeapType(type);
		GFX_CHECK_HR(gfx->GetDevice()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())));
		descriptor_handle_size = gfx->GetDevice()->GetDescriptorHandleIncrementSize(heap_desc.Type);
	}

}