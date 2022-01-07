#pragma once
#include "DynamicAllocation.h"
#include "../Utilities/RingAllocator.h"
#include <mutex>

namespace adria
{

	class RingUploadBuffer
	{
	public:
		RingUploadBuffer(ID3D12Device* device, SIZE_T max_size_in_bytes);

		DynamicAllocation Allocate(SIZE_T size_in_bytes, SIZE_T alignment);

		void FinishCurrentFrame(U64 frame);

		void ReleaseCompletedFrames(U64 completed_frame);

	private:
		RingAllocator ring_allocator;
		std::mutex alloc_mutex;
		Microsoft::WRL::ComPtr<ID3D12Resource> buffer = nullptr;
		BYTE* cpu_address = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;
	};
}