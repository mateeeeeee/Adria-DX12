#pragma once
#include <memory>
#include "Graphics/GfxMacros.h"
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class RenderGraph;

	class GPUDebugPrinter
	{
	public:
		explicit GPUDebugPrinter(GfxDevice* gfx);
		ADRIA_NONCOPYABLE(GPUDebugPrinter)
		ADRIA_DEFAULT_MOVABLE(GPUDebugPrinter)
		~GPUDebugPrinter();

		Int32 GetPrintfBufferIndex();
		void AddClearPass(RenderGraph& rg);
		void AddPrintPass(RenderGraph& rg);

	private:
#if GFX_SHADER_PRINTF
		GfxDevice* gfx;
		std::unique_ptr<GfxBuffer> printf_buffer;
		std::unique_ptr<GfxBuffer> readback_buffers[GFX_BACKBUFFER_COUNT];
		GfxDescriptor srv_descriptor;
		GfxDescriptor uav_descriptor;
		GfxDescriptor gpu_uav_descriptor;
#endif
	};
}