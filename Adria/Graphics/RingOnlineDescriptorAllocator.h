#pragma once
#include "DescriptorHeap.h"
#include "../Utilities/RingAllocator.h"

namespace adria
{

	class RingOnlineDescriptorAllocator : public DescriptorHeap
	{
	public:
		RingOnlineDescriptorAllocator(
			ID3D12DescriptorHeap* pExistingHeap, OffsetType reserve = 0);

		RingOnlineDescriptorAllocator(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_DESC const& desc, OffsetType reserve = 0);

		RingOnlineDescriptorAllocator(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			D3D12_DESCRIPTOR_HEAP_FLAGS flags,
			size_t capacity, OffsetType reserve = 0);

		RingOnlineDescriptorAllocator(
			ID3D12Device* device,
			size_t count, OffsetType reserve = 0);

		~RingOnlineDescriptorAllocator() = default;

		[[nodiscard]] OffsetType Allocate();

		[[nodiscard]] OffsetType AllocateRange(size_t);

		void FinishCurrentFrame(uint64 frame);

		void ReleaseCompletedFrames(uint64 completed_frame);

	private:
		RingAllocator ring_allocator;
		std::mutex alloc_lock;

	};

}