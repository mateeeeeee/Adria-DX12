#pragma once
#include <memory>
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;

	class GPUDebugPrinter
	{
	public:
		explicit GPUDebugPrinter(GfxDevice* gfx);
		~GPUDebugPrinter();

		int32 GetPrintfBufferIndex();

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxBuffer> printf_buffer;
		GfxDescriptor srv_descriptor;
		GfxDescriptor uav_descriptor;
	};
}