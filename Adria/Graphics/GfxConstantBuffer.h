#pragma once
#include "GfxDevice.h"
#include "GfxBuffer.h"

namespace adria
{
	template<typename BufferType>
	class GfxConstantBuffer
	{
		static constexpr uint32 GetCBufferSize()
		{
			return (sizeof(BufferType) + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
		}

	public:
		GfxConstantBuffer(GfxDevice* gfx, uint32 cbuffer_count)
			: cbuffer_size(GetCBufferSize()), cbuffer_count(cbuffer_count)
		{
			GfxBufferDesc desc{};
			desc.resource_usage = GfxResourceUsage::Upload;
			desc.bind_flags = GfxBindFlag::None;
			desc.format = GfxFormat::UNKNOWN;
			desc.misc_flags = GfxBufferMiscFlag::ConstantBuffer;
			desc.size = cbuffer_size * cbuffer_count;
			
			cbuffer = std::make_unique<GfxBuffer>(gfx, desc);
			ADRIA_ASSERT(cbuffer->IsMapped());
			mapped_data = cbuffer->GetMappedData<uint8>();
		}
		ADRIA_NONCOPYABLE(GfxConstantBuffer)
		GfxConstantBuffer(GfxConstantBuffer&& o) noexcept;
		GfxConstantBuffer& operator=(GfxConstantBuffer&&) = delete;
		~GfxConstantBuffer();

		void Update(BufferType const& data, uint32 cbuffer_index);
		void Update(void* data, uint32 data_size, uint32 cbuffer_index);

		uint64 GetGpuAddress(uint32 cbuffer_index) const
		{
			return cbuffer->GetGpuAddress() + (uint64)cbuffer_index * cbuffer_size;
		}

	private:
		std::unique_ptr<GfxBuffer> cbuffer;
		uint8* mapped_data = nullptr;
		uint32 const cbuffer_size;
		uint32 const cbuffer_count;
	};

	template<typename BufferType>
	GfxConstantBuffer<BufferType>::GfxConstantBuffer(GfxConstantBuffer&& o) noexcept
		: cbuffer(std::move(o.cbuffer)), cbuffer_size(o.cbuffer_size), mapped_data(o.mapped_data)
	{
		o.mapped_data = nullptr;
	}
	
	template<typename BufferType>
	GfxConstantBuffer<BufferType>::~GfxConstantBuffer()
	{
		if (cbuffer != nullptr) cbuffer->Unmap();
		mapped_data = nullptr;
	}

	template<typename BufferType>
	void GfxConstantBuffer<BufferType>::Update(BufferType const& data, uint32 cbuffer_index)
	{
		memcpy(&mapped_data[cbuffer_index * cbuffer_size], &data, sizeof(BufferType)); //maybe change to cbuffer_size
	}

	template<typename BufferType>
	void GfxConstantBuffer<BufferType>::Update(void* data, uint32 data_size, uint32 cbuffer_index)
	{
		memcpy(&mapped_data[cbuffer_index * cbuffer_size], data, data_size);
	}

}