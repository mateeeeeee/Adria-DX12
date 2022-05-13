#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <mutex>
#include <vector>
#include "d3dx12.h"
#include "../Core/Macros.h"
#include "../Utilities/LinearAllocator.h"

namespace adria
{

	//typedef struct D3D12_HEAP_DESC
	//{
	//	UINT64 SizeInBytes;
	//	D3D12_HEAP_PROPERTIES Properties;
	//	UINT64 Alignment;
	//	D3D12_HEAP_FLAGS Flags;
	//} 	D3D12_HEAP_DESC;

	class Heap
	{
	public:
		Heap(ID3D12Device* device, D3D12_HEAP_DESC const& heap_desc)
		{
			BREAK_IF_FAILED(device->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap)));
			allocator = std::make_unique<LinearAllocator>(heap_desc.SizeInBytes);
		}

		Heap(ID3D12Device* device, std::vector<D3D12_RESOURCE_DESC> const& resources)
		{
			D3D12_RESOURCE_ALLOCATION_INFO alloc_info = device->GetResourceAllocationInfo(0,
				(UINT)resources.size(), resources.data());

			D3D12_HEAP_DESC heap_desc{};
			heap_desc.Alignment = alloc_info.Alignment;
			heap_desc.SizeInBytes = alloc_info.SizeInBytes;
			heap_desc.Properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			heap_desc.Flags = D3D12_HEAP_FLAG_NONE;
			BREAK_IF_FAILED(device->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap)));
			allocator = std::make_unique<LinearAllocator>(heap_desc.SizeInBytes);
		}

		UINT64 GetAlignment() const { return heap->GetDesc().Alignment; }
		UINT64 GetSize() const { return heap->GetDesc().SizeInBytes; }
		UINT64 Allocate(UINT64 size)
		{
			return allocator->Allocate(size, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		}
		ID3D12Heap* GetHeap() const { return heap.Get(); }
	private:
		Microsoft::WRL::ComPtr<ID3D12Heap> heap;
		std::unique_ptr<LinearAllocator> allocator;
	};
}