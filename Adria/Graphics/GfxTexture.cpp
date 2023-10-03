#include "GfxTexture.h"
#include "GfxDevice.h"
#include "GfxBuffer.h"
#include "GfxCommandList.h"
#include "GfxLinearDynamicAllocator.h"
#include "d3dx12.h"

namespace adria
{

	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureInitialData* initial_data /*= nullptr*/, uint32 subresource_count /*= -1*/) : gfx(gfx), desc(desc)
	{
		HRESULT hr = E_FAIL;
		D3D12MA::ALLOCATION_DESC allocation_desc{};
		allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resource_desc{};
		resource_desc.Format = ConvertGfxFormat(desc.format);
		resource_desc.Width = desc.width;
		resource_desc.Height = desc.height;
		resource_desc.MipLevels = desc.mip_levels;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resource_desc.DepthOrArraySize = (UINT16)desc.array_size;
		resource_desc.SampleDesc.Count = desc.sample_count;
		resource_desc.SampleDesc.Quality = 0;
		resource_desc.Alignment = 0;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (HasAllFlags(desc.bind_flags, GfxBindFlag::DepthStencil))
		{
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			if (!HasAllFlags(desc.bind_flags, GfxBindFlag::ShaderResource))
			{
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}
		}
		if (HasAllFlags(desc.bind_flags, GfxBindFlag::RenderTarget))
		{
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		if (HasAllFlags(desc.bind_flags, GfxBindFlag::UnorderedAccess))
		{
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		switch (desc.type)
		{
		case GfxTextureType_1D:
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			break;
		case GfxTextureType_2D:
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			break;
		case GfxTextureType_3D:
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			resource_desc.DepthOrArraySize = (UINT16)desc.depth;
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Texture Type!");
			break;
		}
		D3D12_CLEAR_VALUE* clear_value_ptr = nullptr;
		D3D12_CLEAR_VALUE clear_value{};
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::DepthStencil) && desc.clear_value.active_member == GfxClearValue::GfxActiveMember::DepthStencil)
		{
			clear_value.DepthStencil.Depth = desc.clear_value.depth_stencil.depth;
			clear_value.DepthStencil.Stencil = desc.clear_value.depth_stencil.stencil;
			switch (desc.format)
			{
			case GfxFormat::R16_TYPELESS:
				clear_value.Format = DXGI_FORMAT_D16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				clear_value.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				clear_value.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
				break;
			default:
				clear_value.Format = ConvertGfxFormat(desc.format);
				break;
			}
			clear_value_ptr = &clear_value;
		}
		else if (HasAnyFlag(desc.bind_flags, GfxBindFlag::RenderTarget) && desc.clear_value.active_member == GfxClearValue::GfxActiveMember::Color)
		{
			clear_value.Color[0] = desc.clear_value.color.color[0];
			clear_value.Color[1] = desc.clear_value.color.color[1];
			clear_value.Color[2] = desc.clear_value.color.color[2];
			clear_value.Color[3] = desc.clear_value.color.color[3];
			switch (desc.format)
			{
			case GfxFormat::R16_TYPELESS:
				clear_value.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				clear_value.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				clear_value.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				clear_value.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				clear_value.Format = ConvertGfxFormat(desc.format);
				break;
			}
			clear_value_ptr = &clear_value;
		}

		D3D12_RESOURCE_STATES resource_state = ConvertGfxResourceStateToD3D12(desc.initial_state);
		if (initial_data != nullptr)
		{
			resource_state = D3D12_RESOURCE_STATE_COMMON;
		}

		auto device = gfx->GetDevice();
		if (desc.heap_type == GfxResourceUsage::Readback || desc.heap_type == GfxResourceUsage::Upload)
		{
			UINT64 required_size = 0;
			device->GetCopyableFootprints(&resource_desc, 0, 1, 0, nullptr, nullptr, nullptr, &required_size);
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Width = required_size;
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			if (desc.heap_type == GfxResourceUsage::Readback)
			{
				allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
				resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
			}
			else if (desc.heap_type == GfxResourceUsage::Upload)
			{
				allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
				resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
			}
		}
		auto allocator = gfx->GetAllocator();

		D3D12MA::Allocation* alloc = nullptr;
		hr = allocator->CreateResource(
			&allocation_desc,
			&resource_desc,
			resource_state,
			clear_value_ptr,
			&alloc,
			IID_PPV_ARGS(resource.GetAddressOf())
		);
		GFX_CHECK_HR(hr);
		allocation.reset(alloc);

		if (desc.heap_type == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		else if (desc.heap_type == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range = {};
			hr = resource->Map(0, &read_range, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		if (desc.mip_levels == 0)
		{
			const_cast<GfxTextureDesc&>(desc).mip_levels = (uint32_t)log2(std::max<uint32>(desc.width, desc.height)) + 1;
		}

		auto upload_buffer = gfx->GetDynamicAllocator();
		auto cmd_list = gfx->GetGraphicsCommandList();
		if (initial_data != nullptr)
		{
			uint64 required_size;
			device->GetCopyableFootprints(&resource_desc, 0, (uint32)subresource_count, 0, nullptr, nullptr, nullptr, &required_size);
			GfxDynamicAllocation dyn_alloc = upload_buffer->Allocate(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
			
			std::vector<D3D12_SUBRESOURCE_DATA> subresource_data(subresource_count);
			for (uint32 i = 0; i < subresource_count; ++i)
			{
				GfxTextureInitialData init_data = initial_data[i];
				subresource_data[i].pData = init_data.data;
				subresource_data[i].RowPitch = init_data.row_pitch;
				subresource_data[i].SlicePitch = init_data.slice_pitch;
			}
			UpdateSubresources(cmd_list->GetNative(), resource.Get(), dyn_alloc.buffer->GetNative(), dyn_alloc.offset, 0, subresource_count, subresource_data.data());

			if (desc.initial_state != GfxResourceState::CopyDest)
			{
				cmd_list->TransitionBarrier(*this, GfxResourceState::CopyDest, desc.initial_state);
				cmd_list->FlushBarriers();
			}
		}
	}

	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, void* backbuffer) 
		: gfx(gfx), desc(desc), resource((ID3D12Resource*)backbuffer), is_backbuffer(true)
	{}

	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureInitialData* initial_data /*= nullptr*/)
		: GfxTexture(gfx, desc, initial_data, desc.array_size* std::max<uint32>(1u, desc.mip_levels))
	{

	}

	GfxTexture::~GfxTexture()
	{
		if (mapped_data != nullptr)
		{
			ADRIA_ASSERT(resource != nullptr);
			resource->Unmap(0, nullptr);
			mapped_data = nullptr;
		}
		if (!is_backbuffer)
		{
			gfx->AddToReleaseQueue(resource.Detach());
			gfx->AddToReleaseQueue(allocation.release());
		}
	}

	uint64 GfxTexture::GetGpuAddress() const
	{
		return resource->GetGPUVirtualAddress();
	}

	ID3D12Resource* GfxTexture::GetNative() const
	{
		return resource.Get();
	}

	GfxTextureDesc const& GfxTexture::GetDesc() const
	{
		return desc;
	}

	bool GfxTexture::IsMapped() const
	{
		return mapped_data != nullptr;
	}

	void* GfxTexture::GetMappedData() const
	{
		return mapped_data;
	}

	void* GfxTexture::Map()
	{
		HRESULT hr;
		if (desc.heap_type == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		else if (desc.heap_type == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range{};
			hr = resource->Map(0, &read_range, &mapped_data);
			GFX_CHECK_HR(hr);
		}
		return mapped_data;
	}

	void GfxTexture::Unmap()
	{
		resource->Unmap(0, nullptr);
	}

	void GfxTexture::SetName(char const* name)
	{
		resource->SetName(ToWideString(name).c_str());
	}
}

