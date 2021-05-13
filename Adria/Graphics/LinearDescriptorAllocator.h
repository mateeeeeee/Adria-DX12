#pragma once
#include "DescriptorHeap.h"
#include "../Utilities/LinearAllocator.h"

namespace adria
{

	
	class LinearDescriptorAllocator : public DescriptorHeap
	{
	public:
		
		LinearDescriptorAllocator(
			ID3D12DescriptorHeap* pExistingHeap, OffsetType reserve = 0);

		LinearDescriptorAllocator(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_DESC const& desc, 
			OffsetType reserve = 0);

		LinearDescriptorAllocator(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			D3D12_DESCRIPTOR_HEAP_FLAGS flags,
			size_t capacity,OffsetType reserve = 0);

		LinearDescriptorAllocator(
			ID3D12Device* device,
			size_t count, OffsetType reserve = 0);

		~LinearDescriptorAllocator() = default;

		[[nodiscard]] OffsetType Allocate();

		[[nodiscard]] OffsetType AllocateRange(size_t);

		void Clear();

	private:
		LinearAllocator linear_allocator;
		std::mutex alloc_lock;
	};
}