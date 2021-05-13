#pragma once
#include "DynamicAllocation.h"
#include "../Utilities/LinearAllocator.h"
#include <mutex>

namespace adria
{

	
	class LinearUploadBuffer
	{
	public:
		LinearUploadBuffer(ID3D12Device* device, SIZE_T max_size_in_bytes);

		DynamicAllocation Allocate(SIZE_T size_in_bytes, SIZE_T alignment);

		void Clear();

	private:
		LinearAllocator linear_allocator;
		std::mutex alloc_mutex;
		Microsoft::WRL::ComPtr<ID3D12Resource> buffer = nullptr;
		BYTE* cpu_address = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;
	};


}