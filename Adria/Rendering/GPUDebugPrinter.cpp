#include "GPUDebugPrinter.h"
#include "Graphics/GfxMacros.h"
#if GFX_SHADER_PRINTF
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Logging/Logger.h"
#include "RenderGraph/RenderGraph.h"
#endif

namespace adria
{
#if GFX_SHADER_PRINTF

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

	constexpr Uint32 ArgCodeSizes[NumDebugPrintArgCodes] =
	{
		4, 8, 12, 16,
		4, 8, 12, 16,
		4, 8, 12, 16
	};
	struct DebugPrintHeader
	{
		Uint32 NumBytes;
		Uint32 StringSize;
		Uint32 NumArgs;
	};
	struct DebugPrintReader
	{
		DebugPrintReader(Uint8* data, Uint32 size) : data(data), size(size), current_offset(0) {}

		Bool HasMoreData(Uint32 count) const
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
		std::string ConsumeString(Uint32 char_count)
		{
			Char* char_data = (Char*)data;
			std::string consumed_string(char_data + current_offset, char_count);
			current_offset += char_count;
			return consumed_string;
		}

		Uint8* data;
		Uint32 const size;
		Uint32 current_offset;
	};

	static std::string MakeArgString(DebugPrintReader& reader, ArgCode arg_code)
	{
		switch (arg_code)
		{
		case DebugPrint_Uint:
		{
			Uint32 value = *reader.Consume<Uint32>();
			return std::to_string(value);
		}
		case DebugPrint_Uint2:
		{
			struct Uint2
			{
				Uint32 value1;
				Uint32 value2;
			};
			Uint2 uint2 = *reader.Consume<Uint2>();
			return "(" + std::to_string(uint2.value1) + "," + std::to_string(uint2.value2) + ")";
		}
		case DebugPrint_Uint3:
		{
			struct Uint3
			{
				Uint32 value1;
				Uint32 value2;
				Uint32 value3;
			};
			Uint3 uint3 = *reader.Consume<Uint3>();
			return "(" + std::to_string(uint3.value1) + "," + std::to_string(uint3.value2) + "," + std::to_string(uint3.value3) + ")";
		}
		case DebugPrint_Uint4:
		{
			struct Uint4
			{
				Uint32 value1;
				Uint32 value2;
				Uint32 value3;
				Uint32 value4;
			};
			Uint4 uint4 = *reader.Consume<Uint4>();
			return "(" + std::to_string(uint4.value1) + "," + std::to_string(uint4.value2) + "," + std::to_string(uint4.value3) + "," + std::to_string(uint4.value4) + ")";
		}
		case DebugPrint_Int:
		{
			Int32 value = *reader.Consume<Int32>();
			return std::to_string(value);
		}
		case DebugPrint_Int2:
		{
			struct Int2
			{
				Int32 value1;
				Int32 value2;
			};
			Int2 int2 = *reader.Consume<Int2>();
			return "(" + std::to_string(int2.value1) + "," + std::to_string(int2.value2) + ")";
		}
		case DebugPrint_Int3:
		{
			struct Int3
			{
				Int32 value1;
				Int32 value2;
				Int32 value3;
			};
			Int3 int3 = *reader.Consume<Int3>();
			return "(" + std::to_string(int3.value1) + "," + std::to_string(int3.value2) + "," + std::to_string(int3.value3) + ")";
		}
		case DebugPrint_Int4:
		{
			struct Int4
			{
				Int32 value1;
				Int32 value2;
				Int32 value3;
				Int32 value4;
			};
			Int4 int4 = *reader.Consume<Int4>();
			return "(" + std::to_string(int4.value1) + "," + std::to_string(int4.value2) + "," + std::to_string(int4.value3) + "," + std::to_string(int4.value4) + ")";
		}
		case DebugPrint_Float:
		{
			Float value = *reader.Consume<Float>();
			return std::to_string(value);
		}
		case DebugPrint_Float2:
		{
			struct Float2
			{
				Float value1;
				Float value2;
			};
			Float2  float2 = *reader.Consume<Float2>();
			return "(" + std::to_string(float2.value1) + "," + std::to_string(float2.value2) + ")";
		}
		case DebugPrint_Float3:
		{
			struct Float3
			{
				Float value1;
				Float value2;
				Float value3;
			};
			Float3 float3 = *reader.Consume<Float3>();
			return "(" + std::to_string(float3.value1) + "," + std::to_string(float3.value2) + "," + std::to_string(float3.value3) + ")";
		}
		case DebugPrint_Float4:
		{
			struct Float4
			{
				Float value1;
				Float value2;
				Float value3;
				Float value4;
			};
			Float4 float4 = *reader.Consume<Float4>();
			return "(" + std::to_string(float4.value1) + "," + std::to_string(float4.value2) + "," + std::to_string(float4.value3) + "," + std::to_string(float4.value4) + ")";
		}
		case NumDebugPrintArgCodes:
		default:
			ADRIA_ASSERT(false);
		}
		return "";
	}


	GPUDebugPrinter::GPUDebugPrinter(GfxDevice* gfx) : gfx(gfx)
	{
		GfxBufferDesc printf_buffer_desc{};
		printf_buffer_desc.stride = sizeof(Uint32);
		printf_buffer_desc.resource_usage = GfxResourceUsage::Default;
		printf_buffer_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		printf_buffer_desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		printf_buffer_desc.size = printf_buffer_desc.stride * 1024 * 1024;
		printf_buffer = std::make_unique<GfxBuffer>(gfx, printf_buffer_desc);
		printf_buffer->SetName("Printf Buffer");

		srv_descriptor = gfx->CreateBufferSRV(printf_buffer.get());
		uav_descriptor = gfx->CreateBufferUAV(printf_buffer.get());

		gfx->GetCommandList()->BufferBarrier(*printf_buffer, GfxResourceState::Common, GfxResourceState::ComputeUAV);

		for (auto& readback_buffer : readback_buffers)
			readback_buffer = gfx->CreateBuffer(ReadBackBufferDesc(printf_buffer_desc.size));
}
	Int32 GPUDebugPrinter::GetPrintfBufferIndex()
	{
		gpu_uav_descriptor = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, gpu_uav_descriptor, uav_descriptor);
		return (Int32)gpu_uav_descriptor.GetIndex();
	}

	void GPUDebugPrinter::AddClearPass(RenderGraph& rg)
	{
		rg.ImportBuffer(RG_NAME(PrintfBuffer), printf_buffer.get());
		struct ClearPrintfBufferPassData
		{
			RGBufferReadWriteId printf_buffer;
		};
		rg.AddPass<ClearPrintfBufferPassData>("Clear Printf Buffer Pass",
			[=](ClearPrintfBufferPassData& data, RenderGraphBuilder& builder)
			{
				data.printf_buffer = builder.WriteBuffer(RG_NAME(PrintfBuffer));
			},
			[=](ClearPrintfBufferPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				Uint32 clear[] = { 0,0,0,0 };
				cmd_list->ClearUAV(*printf_buffer, gpu_uav_descriptor, uav_descriptor, clear);
			}, RGPassType::Compute, RGPassFlags::ForceNoCull);
	}

	void GPUDebugPrinter::AddPrintPass(RenderGraph& rg)
	{
		struct CopyPrintfBufferPassData
		{
			RGBufferCopySrcId printf_buffer;
		};
		rg.AddPass<CopyPrintfBufferPassData>("Copy Printf Buffer Pass",
			[=](CopyPrintfBufferPassData& data, RenderGraphBuilder& builder)
			{
				data.printf_buffer = builder.ReadCopySrcBuffer(RG_NAME(PrintfBuffer));
				std::ignore = builder.ReadCopySrcTexture(RG_NAME(FinalTexture)); //forcing dependency with the final texture so the printf pass doesn't run before some other pass
			},
			[&](CopyPrintfBufferPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				Uint64 current_backbuffer_index = gfx->GetBackbufferIndex();
				GfxBuffer& readback_buffer = *readback_buffers[current_backbuffer_index];
				cmd_list->CopyBuffer(readback_buffer, *printf_buffer);

				Uint64 old_backbuffer_index = (current_backbuffer_index + 1) % gfx->GetBackbufferCount();
				GfxBuffer& old_readback_buffer = *readback_buffers[old_backbuffer_index];

				static constexpr Uint32 MaxDebugPrintArgs = 4;
				DebugPrintReader print_reader(old_readback_buffer.GetMappedData<Uint8>() + sizeof(Uint32), (Uint32)old_readback_buffer.GetSize() - sizeof(Uint32));

				while (print_reader.HasMoreData(sizeof(DebugPrintHeader)))
				{
					DebugPrintHeader const* header = print_reader.Consume<DebugPrintHeader>();
					if (header->NumBytes == 0 || !print_reader.HasMoreData(header->NumBytes))
						break;

					std::string fmt = print_reader.ConsumeString(header->StringSize);
					if (fmt.length() == 0) break;

					if (header->NumArgs > MaxDebugPrintArgs) break;

					std::vector<std::string> arg_strings;
					arg_strings.reserve(header->NumArgs);
					for (Uint32 arg_idx = 0; arg_idx < header->NumArgs; ++arg_idx)
					{
						ArgCode const arg_code = (ArgCode)*print_reader.Consume<Uint8>();
						if (arg_code >= NumDebugPrintArgCodes || arg_code < 0) break;

						Uint32 const arg_size = ArgCodeSizes[arg_code];
						if (!print_reader.HasMoreData(arg_size)) break;

						std::string const arg_string = MakeArgString(print_reader, arg_code);
						arg_strings.push_back(arg_string);
					}

					if (header->NumArgs > 0)
					{
						for (Uint64 i = 0; i < arg_strings.size(); ++i)
						{
							std::string placeholder = "{" + std::to_string(i) + "}";
							Uint64 pos = fmt.find(placeholder);
							while (pos != std::string::npos)
							{
								fmt.replace(pos, placeholder.length(), arg_strings[i]);
								pos = fmt.find(placeholder, pos + arg_strings[i].length());
							}
						}
					}
					ADRIA_LOG(DEBUG, fmt.c_str());
				}
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);
	}
#else
	GPUDebugPrinter::GPUDebugPrinter(GfxDevice* gfx) : gfx(gfx) {}
	Int32 GPUDebugPrinter::GetPrintfBufferIndex() { return -1; }
	void GPUDebugPrinter::AddClearPass(RenderGraph& rg) {}
	void GPUDebugPrinter::AddPrintPass(RenderGraph& rg) {}
#endif
	GPUDebugPrinter::~GPUDebugPrinter() = default;
}

