#include "GPUDebugPrinter.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Logging/Logger.h"

namespace adria
{
	enum ArgCode
	{
		DebugPrint_Uint = 0,
		DebugPrint_Uint2,
		DebugPrint_Uint3,
		DebugPrint_Uint4,
		DebugPrint_Int,
		DebugPrint_Int2,
		DebugPrint_Int3,
		DebugPrint_Int4,
		DebugPrint_Float,
		DebugPrint_Float2,
		DebugPrint_Float3,
		DebugPrint_Float4,
		NumDebugPrintArgCodes
	};

	struct DebugPrintHeader
	{
		uint32 NumBytes;
		uint32 StringSize;
		uint32 NumArgs;
	};

	struct DebugPrintReader
	{
		DebugPrintReader(uint8* data, uint32 size) : data(data), size(size), current_offset(0) {}

		bool HasMoreData(uint32 count)
		{
			return current_offset + count <= size;
		}

		template<typename T>
		T* Consume()
		{
			T* consumed_data = reinterpret_cast<T*>(data + current_offset);
			current_offset += sizeof(T);
			return consumed_data;
		}

		std::string ConsumeString(uint32 char_count) 
		{
			char* char_data = (char*)data;
			std::string consumed_string(char_data + current_offset, char_count);
			current_offset += char_count;
			return consumed_string;
		}

		uint8* data;
		uint32 const size;
		uint32 current_offset;
	};

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

		gfx->GetCommandList()->TransitionBarrier(*printf_buffer, GfxResourceState::Common, GfxResourceState::UnorderedAccess);

		for (auto& readback_buffer : readback_buffers)
			readback_buffer = gfx->CreateBuffer(ReadBackBufferDesc(printf_buffer_desc.size));
	}

	GPUDebugPrinter::~GPUDebugPrinter() = default;

	int32 GPUDebugPrinter::GetPrintfBufferIndex()
	{
		GfxCommandList* cmd_list = gfx->GetLatestCommandList(GfxCommandListType::Graphics);
		GfxDescriptor gpu_uav_descriptor = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, gpu_uav_descriptor, uav_descriptor);
		uint32 clear[] = { 0,0,0,0 };
		cmd_list->ClearUAV(*printf_buffer, gpu_uav_descriptor, uav_descriptor, clear);
		return (int32)gpu_uav_descriptor.GetIndex();
	}

	void GPUDebugPrinter::Print()
	{
		GfxCommandList* cmd_list = gfx->GetLatestCommandList(GfxCommandListType::Graphics);
		cmd_list->TransitionBarrier(*printf_buffer, GfxResourceState::UnorderedAccess, GfxResourceState::CopySource);

		uint64 current_backbuffer_index = gfx->GetBackbufferIndex();
		GfxBuffer& readback_buffer = *readback_buffers[current_backbuffer_index];
		cmd_list->CopyBuffer(readback_buffer, *printf_buffer);

		uint64 old_backbuffer_index = (current_backbuffer_index + 1) % gfx->GetBackbufferCount();
		GfxBuffer& old_readback_buffer = *readback_buffers[old_backbuffer_index];

		static constexpr uint32 MaxDebugPrintArgs = 4;
		DebugPrintReader print_reader(old_readback_buffer.GetMappedData<uint8>() + sizeof(uint32), (uint32)old_readback_buffer.GetSize() - sizeof(uint32));

		while (print_reader.HasMoreData(sizeof(DebugPrintHeader)))
		{
			DebugPrintHeader const* header = print_reader.Consume<DebugPrintHeader>();
			if (header->NumBytes == 0 || !print_reader.HasMoreData(header->NumBytes))
				break;

			std::string fmt = print_reader.ConsumeString(header->StringSize);
			if (fmt.length() == 0) break;

			if (header->NumArgs > MaxDebugPrintArgs) break;
			ADRIA_LOG(DEBUG, fmt.c_str());
		}
		cmd_list->TransitionBarrier(*printf_buffer, GfxResourceState::CopySource, GfxResourceState::UnorderedAccess);
	}

}

