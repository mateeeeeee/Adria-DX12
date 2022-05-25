#include "RingOnlineDescriptorAllocator.h"



namespace adria
{
	RingOnlineDescriptorAllocator::RingOnlineDescriptorAllocator(ID3D12DescriptorHeap* pExistingHeap, OffsetType reserve)
		: DescriptorHeap(pExistingHeap), ring_allocator(Count(), reserve)
	{
	}
	RingOnlineDescriptorAllocator::RingOnlineDescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_DESC const& desc
		, OffsetType reserve) : DescriptorHeap(device, desc),
		ring_allocator(Count(),reserve)
	{
	}
	RingOnlineDescriptorAllocator::RingOnlineDescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, size_t capacity, OffsetType reserve)
		: DescriptorHeap(device, type, flags, capacity), ring_allocator(Count(), reserve)
	{
	}
	RingOnlineDescriptorAllocator::RingOnlineDescriptorAllocator(ID3D12Device* device, size_t count, OffsetType reserve)
		: RingOnlineDescriptorAllocator(device,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, count, reserve)
	{
	}

	[[nodiscard]]
	OffsetType RingOnlineDescriptorAllocator::Allocate()
	{
		return AllocateRange(1);
	}

	[[nodiscard]]
	OffsetType RingOnlineDescriptorAllocator::AllocateRange(size_t range_size)
	{
		OffsetType start = INVALID_OFFSET;
		{
			std::lock_guard<std::mutex> guard(alloc_lock);
			start = ring_allocator.Allocate(range_size);
		}
		ADRIA_ASSERT(start != INVALID_OFFSET && "Don't have enough space");

		return start;
	}

	void RingOnlineDescriptorAllocator::FinishCurrentFrame(uint64 frame)
	{
		std::lock_guard<std::mutex> guard(alloc_lock);
		ring_allocator.FinishCurrentFrame(frame);
	}

	void RingOnlineDescriptorAllocator::ReleaseCompletedFrames(uint64 completed_frame)
	{
		std::lock_guard<std::mutex> guard(alloc_lock);
		ring_allocator.ReleaseCompletedFrames(completed_frame);
	}

}