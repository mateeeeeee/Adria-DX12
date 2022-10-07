#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"
#include "../Core/Macros.h"
#include "../Core/Definitions.h"
#include "../Utilities/AllocatorUtil.h"

namespace adria
{
	template<typename T>
	concept NotPointer = !std::is_pointer_v<T>;

	template<typename CBuffer>
	inline constexpr uint32 GetCBufferSize()
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

		void Update(void const* data, size_t size, size_t offset = 0)
		{
			memcpy((uint8*)cpu_address + offset, data, size);
		}

		template<typename T> 
		void Update(T const& data)
		{
			memcpy(cpu_address, &data, sizeof(T));
		}
	};

	struct AllocDesc
	{
		size_t size_in_bytes;
		size_t alignment;
	};
}