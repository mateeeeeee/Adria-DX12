#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"
#include "../Core/Macros.h"
#include "../Core/Definitions.h"
#include "../Utilities/AllocatorUtil.h"

namespace adria
{

	template<typename CBuffer>
	inline constexpr u32 GetCBufferSize()
	{
		return (sizeof(CBuffer) + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
	}

	struct DynamicAllocation
	{
		ID3D12Resource* buffer = nullptr;
		void* cpu_address = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpu_address = 0;
		OffsetType offset = 0;
		OffsetType size = 0;

		void Update(void* data, size_t size)
		{
			memcpy(cpu_address, data, size);
		}

		template<typename T>
		void Update(T const& data)
		{
			memcpy(cpu_address, &data, sizeof(T));
		}
	};

}