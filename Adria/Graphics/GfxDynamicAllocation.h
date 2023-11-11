#pragma once
#include "Utilities/AllocatorUtil.h"

namespace adria
{
	class GfxBuffer;

	struct GfxDynamicAllocation
	{
		GfxBuffer* buffer = nullptr;
		void* cpu_address = nullptr;
		uint64 gpu_address = 0;
		OffsetType offset = 0;
		OffsetType size = 0;

		void Update(void const* data, uint64 size, uint64 offset = 0)
		{
			memcpy((uint8*)cpu_address + offset, data, size);
		}

		template<typename T>
		void Update(T const& data)
		{
			memcpy(cpu_address, &data, sizeof(T));
		}
	};
}