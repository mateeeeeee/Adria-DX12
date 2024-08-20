#include "GfxBuffer.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "GfxLinearDynamicAllocator.h"

#include <format>

namespace adria
{

	GfxBuffer::GfxBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data) : gfx(gfx), desc(desc)
	{
		UINT64 buffer_size = desc.size;
		if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::ConstantBuffer))
			buffer_size = Align(buffer_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		D3D12_RESOURCE_DESC resource_desc{};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.Width = buffer_size;
		resource_desc.Height = 1;
		resource_desc.MipLevels = 1;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.Alignment = 0;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.SampleDesc.Quality = 0;

		if (HasAllFlags(desc.bind_flags, GfxBindFlag::UnorderedAccess))
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		if (!HasAllFlags(desc.bind_flags, GfxBindFlag::ShaderResource))
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::AccelStruct))
			resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		D3D12MA::ALLOCATION_DESC allocation_desc{};
		allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
			resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
		{
			allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
		}

		auto device = gfx->GetDevice();
		auto allocator = gfx->GetAllocator();

		D3D12MA::Allocation* alloc = nullptr;
		HRESULT hr = allocator->CreateResource(
			&allocation_desc,
			&resource_desc,
			resource_state,
			nullptr,
			&alloc,
			IID_PPV_ARGS(resource.GetAddressOf())
		);
		GFX_CHECK_HR(hr);
		allocation.reset(alloc);

		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range{};
			hr = resource->Map(0, &read_range, &mapped_data);
			GFX_CHECK_HR(hr);

			if (initial_data)
			{
				memcpy(mapped_data, initial_data, desc.size);
			}
		}

		if (initial_data != nullptr && desc.resource_usage != GfxResourceUsage::Upload)
		{
			auto cmd_list = gfx->GetCommandList();
			auto upload_buffer = gfx->GetDynamicAllocator();
			GfxDynamicAllocation upload_alloc = upload_buffer->Allocate(buffer_size);
			upload_alloc.Update(initial_data, desc.size);
			cmd_list->CopyBuffer(
				*this,
				0,
				*upload_alloc.buffer,
				upload_alloc.offset,
				desc.size);

			if (HasAnyFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
			{
				cmd_list->BufferBarrier(*this, GfxResourceState::CopyDst, GfxResourceState::AllSRV);
				cmd_list->FlushBarriers();
			}
		}
	}

	GfxBuffer::~GfxBuffer()
	{
		if (mapped_data != nullptr)
		{
			ADRIA_ASSERT(resource != nullptr);
			resource->Unmap(0, nullptr);
			mapped_data = nullptr;
		}
	}

	void* GfxBuffer::GetMappedData() const
	{
		return mapped_data;
	}

	ID3D12Resource* GfxBuffer::GetNative() const
	{
		return resource.Get();
	}

	GfxBufferDesc const& GfxBuffer::GetDesc() const
	{
		return desc;
	}

	uint64 GfxBuffer::GetGpuAddress() const
	{
		return resource->GetGPUVirtualAddress();
	}

	uint64 GfxBuffer::GetSize() const
	{
		return desc.size;
	}

	uint32 GfxBuffer::GetStride() const
	{
		return desc.stride;
	}

	uint32 GfxBuffer::GetCount() const
	{
		ADRIA_ASSERT(desc.stride != 0);
		return static_cast<uint32>(desc.size / desc.stride);
	}

	GfxFormat GfxBuffer::GetFormat() const
	{
		return desc.format;
	}

	bool GfxBuffer::IsMapped() const
	{
		return mapped_data != nullptr;
	}

	void* GfxBuffer::Map()
	{
		if (mapped_data) return mapped_data;

		HRESULT hr;
		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range{};
			hr = resource->Map(0, &read_range, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		return mapped_data;
	}

	void GfxBuffer::Unmap()
	{
		resource->Unmap(0, nullptr);
		mapped_data = nullptr;
	}

	void GfxBuffer::Update(void const* src_data, uint64 data_size, uint64 offset /*= 0*/)
	{
		ADRIA_ASSERT(desc.resource_usage == GfxResourceUsage::Upload);
		if (mapped_data)
		{
			memcpy((uint8*)mapped_data + offset, src_data, data_size);
		}
		else
		{
			Map();
			ADRIA_ASSERT(mapped_data);
			memcpy((uint8*)mapped_data + offset, src_data, data_size);
		}
	}

	void GfxBuffer::SetName(char const* name)
	{
		resource->SetName(ToWideString(name).c_str());
	}
}