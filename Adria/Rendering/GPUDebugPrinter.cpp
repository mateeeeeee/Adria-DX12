#include "GPUDebugPrinter.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"

namespace adria
{

	GPUDebugPrinter::GPUDebugPrinter(GfxDevice* gfx) : gfx(gfx)
	{
		GfxBufferDesc printf_buffer_desc{};
		printf_buffer_desc.stride = sizeof(uint32);
		printf_buffer_desc.resource_usage = GfxResourceUsage::Default;
		printf_buffer_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		printf_buffer_desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		printf_buffer_desc.size = printf_buffer_desc.stride * 1024 * 1024;
		printf_buffer = std::make_unique<GfxBuffer>(gfx, printf_buffer_desc);

		srv_descriptor = gfx->CreateBufferSRV(printf_buffer.get());
		uav_descriptor = gfx->CreateBufferUAV(printf_buffer.get());
	}

	GPUDebugPrinter::~GPUDebugPrinter()
	{
	}

	int32 GPUDebugPrinter::GetPrintfBufferIndex()
	{
		GfxCommandList* cmd_list = gfx->GetLatestCommandList(GfxCommandListType::Graphics);
		GfxDescriptor gpu_uav_descriptor = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, gpu_uav_descriptor, uav_descriptor);
		uint32 clear[] = { 0,0,0,0 };
		cmd_list->ClearUAV(*printf_buffer, gpu_uav_descriptor, uav_descriptor, clear);
		return (int32)gpu_uav_descriptor.GetIndex();
	}

}

