#include "LinearDescriptorAllocator.h"


namespace adria
{
	LinearDescriptorAllocator::LinearDescriptorAllocator(ID3D12DescriptorHeap* pExistingHeap, OffsetType reserve)
		: DescriptorHeap(pExistingHeap), linear_allocator(Count(), reserve)
		
	{
	}

	LinearDescriptorAllocator::LinearDescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_DESC const& desc
		, OffsetType reserve)
		: DescriptorHeap(device, desc), linear_allocator(Count(), reserve)
		
	{
	}
	LinearDescriptorAllocator::LinearDescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, size_t capacity
		, OffsetType reserve)
		: DescriptorHeap(device, type, flags, capacity), linear_allocator(Count(), reserve)
	{
	}
	LinearDescriptorAllocator::LinearDescriptorAllocator(ID3D12Device* device, size_t count, OffsetType reserve) :
		LinearDescriptorAllocator(device,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, count, reserve) {}
	
	[[nodiscard]]
	OffsetType LinearDescriptorAllocator::Allocate()
	{
		return AllocateRange(1);
	}

	[[nodiscard]]
	OffsetType LinearDescriptorAllocator::AllocateRange(size_t num_descriptors)
	{
		OffsetType start = INVALID_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_lock);
			start = linear_allocator.Allocate(num_descriptors);
		}
		ADRIA_ASSERT(start != INVALID_OFFSET && "Don't have enough space");

		return start;
	}

	void LinearDescriptorAllocator::Clear()
	{
		std::lock_guard<std::mutex> guard(alloc_lock);
		linear_allocator.Clear();
	}
}