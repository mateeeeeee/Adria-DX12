#pragma once
#include <memory>
#include "GfxDevice.h"
#include "d3dx12.h"
#include "../Core/CoreTypes.h"
#include "../Core/Defines.h"

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
		GfxConstantBuffer(ID3D12Device* device, uint32 cbuffer_count)
			: cbuffer_size(GetCBufferSize()), cbuffer_count(cbuffer_count)
		{
			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer((uint64)cbuffer_size * cbuffer_count);

			BREAK_IF_FAILED(device->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&buffer_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(cb.GetAddressOf())));

			CD3DX12_RANGE read_range(0, 0);
			BREAK_IF_FAILED(cb->Map(0, &read_range, reinterpret_cast<void**>(&_mapped_data)));
		}

		GfxConstantBuffer(GfxConstantBuffer const&) = delete;
		GfxConstantBuffer(GfxConstantBuffer&& o) noexcept;

		GfxConstantBuffer& operator=(GfxConstantBuffer const&) = delete;
		GfxConstantBuffer& operator=(GfxConstantBuffer&&) = delete;

		~GfxConstantBuffer();

		void Update(BufferType const& data, uint32 cbuffer_index);
		void Update(void* data, uint32 data_size, uint32 cbuffer_index);

		D3D12_CONSTANT_BUFFER_VIEW_DESC View(uint32 cbuffer_index) const;
		D3D12_GPU_VIRTUAL_ADDRESS BufferLocation(uint32 cbuffer_index) const
		{
			return View(cbuffer_index).BufferLocation;
		}
		ID3D12Resource* Resource() const 
		{
			return cb.Get();
		}
	private:
		ArcPtr<ID3D12Resource> cb;
		uint8* _mapped_data = nullptr;
		uint32 const cbuffer_size;
		uint32 const cbuffer_count;
	};

	template<typename BufferType>
	GfxConstantBuffer<BufferType>::GfxConstantBuffer(GfxConstantBuffer&& o) noexcept
		: cb(std::move(o.cb)), cbuffer_size(o.cbuffer_size), _mapped_data(o._mapped_data)
	{
		o._mapped_data = nullptr;
	}
	
	template<typename BufferType>
	GfxConstantBuffer<BufferType>::~GfxConstantBuffer()
	{
		if (cb != nullptr)
			cb->Unmap(0, nullptr);

		_mapped_data = nullptr;
	}

	template<typename BufferType>
	void GfxConstantBuffer<BufferType>::Update(BufferType const& data, uint32 cbuffer_index)
	{
		memcpy(&_mapped_data[cbuffer_index * cbuffer_size], &data, sizeof(BufferType)); //maybe change to cbuffer_size
	}

	template<typename BufferType>
	void GfxConstantBuffer<BufferType>::Update(void* data, uint32 data_size, uint32 cbuffer_index)
	{
		memcpy(&_mapped_data[cbuffer_index * cbuffer_size], data, data_size);
	}

	template<typename BufferType>
	D3D12_CONSTANT_BUFFER_VIEW_DESC GfxConstantBuffer<BufferType>::View(uint32 cbuffer_index) const
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
		cbv_desc.BufferLocation = cb->GetGPUVirtualAddress() + (uint64)cbuffer_index * cbuffer_size;
		cbv_desc.SizeInBytes = cbuffer_size;
		return cbv_desc;
	}





}