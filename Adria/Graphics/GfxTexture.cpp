#include "GfxTexture.h"
#include "GfxDevice.h"
#include "GfxBuffer.h"
#include "GfxCommandList.h"
#include "GfxLinearDynamicAllocator.h"
#include "d3dx12.h"

namespace adria
{
	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data) : gfx(gfx), desc(desc)
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
		resource_desc.DepthOrArraySize = (uint16)desc.array_size;
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

		GfxResourceState initial_state = desc.initial_state;
		if (data.sub_data != nullptr)
		{
			initial_state = GfxResourceState::CopyDst;
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
				initial_state = GfxResourceState::CopyDst;
			}
			else if (desc.heap_type == GfxResourceUsage::Upload)
			{
				allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
				initial_state = GfxResourceState::GenericRead;
			}
		}
		auto allocator = gfx->GetAllocator();

		D3D12MA::Allocation* alloc = nullptr;
		if (gfx->GetCapabilities().SupportsEnhancedBarriers())
		{
			D3D12_RESOURCE_DESC1 resource_desc1 = CD3DX12_RESOURCE_DESC1(resource_desc);
			hr = allocator->CreateResource3(
				&allocation_desc,
				&resource_desc1,
				ToD3D12BarrierLayout(initial_state),
				clear_value_ptr, 0, nullptr,
				&alloc,
				IID_PPV_ARGS(resource.GetAddressOf())
			);
		}
		else
		{
			hr = allocator->CreateResource(
				&allocation_desc,
				&resource_desc,
				ToD3D12LegacyResourceState(initial_state),
				clear_value_ptr,
				&alloc,
				IID_PPV_ARGS(resource.GetAddressOf())
			);
		}
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

		auto dynamic_allocator = gfx->GetDynamicAllocator();
		auto cmd_list = gfx->GetCommandList();
		if (data.sub_data != nullptr)
		{
			uint32 subresource_count = data.sub_count;
			if (subresource_count == uint32(-1)) subresource_count = desc.array_size * std::max<uint32>(1u, desc.mip_levels);
			uint64 required_size;
			device->GetCopyableFootprints(&resource_desc, 0, (uint32)subresource_count, 0, nullptr, nullptr, nullptr, &required_size);
			GfxDynamicAllocation dyn_alloc = dynamic_allocator->Allocate(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
			
			std::vector<D3D12_SUBRESOURCE_DATA> subresource_data(subresource_count);
			for (uint32 i = 0; i < subresource_count; ++i)
			{
				GfxTextureSubData init_data = data.sub_data[i];
				subresource_data[i].pData = init_data.data;
				subresource_data[i].RowPitch = init_data.row_pitch;
				subresource_data[i].SlicePitch = init_data.slice_pitch;
			}
			UpdateSubresources(cmd_list->GetNative(), resource.Get(), dyn_alloc.buffer->GetNative(), dyn_alloc.offset, 0, subresource_count, subresource_data.data());

			if (desc.initial_state != GfxResourceState::CopyDst)
			{
				cmd_list->TextureBarrier(*this, GfxResourceState::CopyDst, desc.initial_state);
				cmd_list->FlushBarriers();
			}
		}
	}

	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, void* backbuffer) 
		: gfx(gfx), desc(desc), resource((ID3D12Resource*)backbuffer), is_backbuffer(true)
	{}

	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc) : GfxTexture(gfx, desc, GfxTextureData{})
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

