#pragma once
#include "FidelityFX/host/backends/dx12/ffx_dx12.h"

namespace adria
{
	class GfxBuffer;
	class GfxTexture;

	FfxResource GetFfxResource();
	FfxResource GetFfxResource(GfxBuffer const& buffer, FfxResourceStates state = FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ, FfxResourceUsage additional_usage = FFX_RESOURCE_USAGE_READ_ONLY);
	FfxResource GetFfxResource(GfxTexture const& texture, FfxResourceStates state = FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ, FfxResourceUsage additional_usage = FFX_RESOURCE_USAGE_READ_ONLY);
}