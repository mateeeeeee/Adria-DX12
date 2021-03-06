#pragma once
#include "DynamicAllocation.h"
#include "../Utilities/LinearAllocator.h"
#include <mutex>

namespace adria
{

	
	class LinearDynamicAllocator
	{
	public:
		LinearDynamicAllocator(ID3D12Device* device, SIZE_T max_size_in_bytes);

		DynamicAllocation Allocate(SIZE_T size_in_bytes, SIZE_T alignment = 0);

		void Clear();

		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress() const;

	private:
		LinearAllocator linear_allocator;
		std::mutex alloc_mutex;
		Microsoft::WRL::ComPtr<ID3D12Resource> buffer = nullptr;
		BYTE* cpu_address = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;
	};


}