#pragma once
#include <list>
#include "DescriptorHeap.h"
#include "../Utilities/AllocatorUtil.h"


namespace adria
{
	class OfflineDescriptorAllocator : public DescriptorHeap
	{
		struct DescriptorRange
		{
			D3D12_CPU_DESCRIPTOR_HANDLE begin;
			D3D12_CPU_DESCRIPTOR_HANDLE end;
		};

	public:
		OfflineDescriptorAllocator(ID3D12DescriptorHeap* pExistingHeap) : DescriptorHeap(pExistingHeap)
		{
			InitList();
		}

		OfflineDescriptorAllocator(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_DESC const& _desc) : DescriptorHeap(device, _desc)
		{
			InitList();
		}

		OfflineDescriptorAllocator(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			D3D12_DESCRIPTOR_HEAP_FLAGS flags,
			size_t count) : DescriptorHeap(device, type, flags, count)
		{
			InitList();
		}

		OfflineDescriptorAllocator(
			ID3D12Device* device,
			size_t count) : DescriptorHeap(device, count)
		{
			InitList();
		}

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor()
		{
			DescriptorRange& range = free_descriptor_ranges.front();
			D3D12_CPU_DESCRIPTOR_HANDLE handle = range.begin;
			range.begin.ptr += Increment();
			if (range.begin.ptr == range.end.ptr) free_descriptor_ranges.pop_front();
			return handle;
		}

		void FreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			DescriptorRange rng{
				.begin = handle,
				.end = D3D12_CPU_DESCRIPTOR_HANDLE{.ptr = handle.ptr + Increment()}
			};

			bool found = false;
			for (auto range = std::begin(free_descriptor_ranges);
					  range != std::end(free_descriptor_ranges) && found == false; ++range)
			{
				if (range->begin.ptr == handle.ptr + Increment())
				{
					range->begin = handle;
					found = true;
				}
				else if (range->end.ptr == handle.ptr)
				{
					range->end.ptr += Increment();
					found = true;
				}
				else if(range->begin.ptr > handle.ptr)
				{
					free_descriptor_ranges.insert(range, rng);
					found = true;
				}
			}

			if (!found) free_descriptor_ranges.push_back(rng);
		}

	private:
		std::list<DescriptorRange> free_descriptor_ranges;

	private: 
		void InitList()
		{
			free_descriptor_ranges.push_back(DescriptorRange{ .begin = hCPU, .end = D3D12_CPU_DESCRIPTOR_HANDLE{.ptr = hCPU.ptr + descriptor_handle_size * Count()} });
		}
	};
}