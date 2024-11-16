#pragma once
#include "GfxDevice.h"
#include "GfxBuffer.h"

namespace adria
{
	template<typename CBufferT>
	class GfxConstantBuffer
	{
		static constexpr Uint32 GetCBufferSize()
		{
			return (sizeof(CBufferT) + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
		}

	public:
		GfxConstantBuffer(GfxDevice* gfx, Uint32 cbuffer_count)
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
			mapped_data = cbuffer->GetMappedData<Uint8>();
		}
		ADRIA_NONCOPYABLE(GfxConstantBuffer)
		GfxConstantBuffer(GfxConstantBuffer&& o) noexcept;
		GfxConstantBuffer& operator=(GfxConstantBuffer&&) = delete;
		~GfxConstantBuffer();

		void Update(CBufferT const& data, Uint32 cbuffer_index);
		void Update(void* data, Uint32 data_size, Uint32 cbuffer_index);

		Uint64 GetGpuAddress(Uint32 cbuffer_index) const
		{
			return cbuffer->GetGpuAddress() + (Uint64)cbuffer_index * cbuffer_size;
		}

	private:
		std::unique_ptr<GfxBuffer> cbuffer;
		Uint8* mapped_data = nullptr;
		Uint32 const cbuffer_size;
		Uint32 const cbuffer_count;
	};

	template<typename CBufferT>
	GfxConstantBuffer<CBufferT>::GfxConstantBuffer(GfxConstantBuffer&& o) noexcept
		: cbuffer(std::move(o.cbuffer)), cbuffer_size(o.cbuffer_size), mapped_data(o.mapped_data)
	{
		o.mapped_data = nullptr;
	}
	
	template<typename CBufferT>
	GfxConstantBuffer<CBufferT>::~GfxConstantBuffer()
	{
		if (cbuffer != nullptr) cbuffer->Unmap();
		mapped_data = nullptr;
	}

	template<typename CBufferT>
	void GfxConstantBuffer<CBufferT>::Update(CBufferT const& data, Uint32 cbuffer_index)
	{
		memcpy(&mapped_data[cbuffer_index * cbuffer_size], &data, sizeof(CBufferT)); //maybe change to cbuffer_size
	}

	template<typename CBufferT>
	void GfxConstantBuffer<CBufferT>::Update(void* data, Uint32 data_size, Uint32 cbuffer_index)
	{
		memcpy(&mapped_data[cbuffer_index * cbuffer_size], data, data_size);
	}

}