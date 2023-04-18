#include "GfxRayTracingAS.h"
#include "GfxDevice.h"
#include "GfxBuffer.h"

namespace adria
{

	uint64 GfxRayTracingBLAS::GetGpuAddress() const
	{
		return as_buffer->GetGpuAddress();
	}

	uint64 GfxRayTracingTLAS::GetGpuAddress() const
	{
		return as_buffer->GetGpuAddress();
	}

}

