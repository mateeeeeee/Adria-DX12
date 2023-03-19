#include "GfxTexture.h"
#include "LinearDynamicAllocator.h"

namespace adria
{

	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureInitialData* initial_data /*= nullptr*/, size_t data_count /*= -1*/) : gfx(gfx), desc(desc)
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

		D3D12_RESOURCE_STATES resource_state = ConvertToD3D12ResourceState(desc.initial_state);
		if (initial_data != nullptr)
		{
			resource_state = D3D12_RESOURCE_STATE_COMMON;
		}

		auto device = gfx->GetDevice();
		if (desc.heap_type == GfxResourceUsage::Readback || desc.heap_type == GfxResourceUsage::Upload)
		{
			UINT64 required_size = 0;
			device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &footprint, nullptr, nullptr, &required_size);
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
		BREAK_IF_FAILED(hr);
		allocation.reset(alloc);

		if (desc.heap_type == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			BREAK_IF_FAILED(hr);
			mapped_rowpitch = footprint.Footprint.RowPitch;
		}
		else if (desc.heap_type == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range = {};
			hr = resource->Map(0, &read_range, &mapped_data);
			BREAK_IF_FAILED(hr);
			mapped_rowpitch = footprint.Footprint.RowPitch;
		}
		if (desc.mip_levels == 0)
		{
			const_cast<GfxTextureDesc&>(desc).mip_levels = (uint32_t)log2(std::max<uint32>(desc.width, desc.height)) + 1;
		}

		auto upload_buffer = gfx->GetDynamicAllocator();
		auto cmd_list = gfx->GetCommandList();
		if (initial_data != nullptr)
		{
			if (data_count == static_cast<size_t>(-1))
			{
				data_count = desc.array_size * std::max<uint32>(1u, desc.mip_levels);
			}
			UINT64 required_size;
			device->GetCopyableFootprints(&resource_desc, 0, (UINT)data_count, 0, nullptr, nullptr, nullptr, &required_size);
			DynamicAllocation dyn_alloc = upload_buffer->Allocate(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
			UpdateSubresources(cmd_list, resource.Get(), dyn_alloc.buffer,
				dyn_alloc.offset, 0, (UINT)data_count, initial_data);

			if (desc.initial_state != GfxResourceState::CopyDest)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = resource.Get();
				barrier.Transition.StateAfter = ConvertToD3D12ResourceState(desc.initial_state);
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				cmd_list->ResourceBarrier(1, &barrier);
			}	
		}
	}

	GfxTexture::GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, ID3D12Resource* backbuffer) : gfx(gfx), desc(desc), resource(backbuffer)
	{}

	GfxTexture::~GfxTexture()
	{
		if (mapped_data != nullptr)
		{
			ADRIA_ASSERT(resource != nullptr);
			resource->Unmap(0, nullptr);
			mapped_data = nullptr;
		}

		for (auto& srv : srvs) gfx->FreeOfflineDescriptor(srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto& uav : uavs) gfx->FreeOfflineDescriptor(uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto& rtv : rtvs) gfx->FreeOfflineDescriptor(rtv, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (auto& dsv : dsvs) gfx->FreeOfflineDescriptor(dsv, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	size_t GfxTexture::CreateSRV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		return CreateSubresource(GfxSubresourceType::SRV, _desc);
	}

	size_t GfxTexture::CreateUAV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		return CreateSubresource(GfxSubresourceType::UAV, _desc);
	}

	size_t GfxTexture::CreateRTV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		return CreateSubresource(GfxSubresourceType::RTV, _desc);
	}

	size_t GfxTexture::CreateDSV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		return CreateSubresource(GfxSubresourceType::DSV, _desc);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::TakeSRV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		size_t i = CreateSubresource(GfxSubresourceType::SRV, _desc);
		ADRIA_ASSERT(srvs.size() - 1 == i);
		D3D12_CPU_DESCRIPTOR_HANDLE srv = srvs.back();
		srvs.pop_back();
		return srv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::TakeUAV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		size_t i = CreateSubresource(GfxSubresourceType::UAV, _desc);
		ADRIA_ASSERT(uavs.size() - 1 == i);
		D3D12_CPU_DESCRIPTOR_HANDLE uav = uavs.back();
		uavs.pop_back();
		return uav;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::TakeRTV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		size_t i = CreateSubresource(GfxSubresourceType::RTV, _desc);
		ADRIA_ASSERT(rtvs.size() - 1 == i);
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvs.back();
		rtvs.pop_back();
		return rtv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::TakeDSV(GfxTextureSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxTextureSubresourceDesc _desc = desc ? *desc : GfxTextureSubresourceDesc{};
		size_t i = CreateSubresource(GfxSubresourceType::DSV, _desc);
		ADRIA_ASSERT(dsvs.size() - 1 == i);
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvs.back();
		dsvs.pop_back();
		return dsv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::GetSRV(size_t i /*= 0*/) const
	{
		return GetSubresource(GfxSubresourceType::SRV, i);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::GetUAV(size_t i /*= 0*/) const
	{
		return GetSubresource(GfxSubresourceType::UAV, i);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::GetRTV(size_t i /*= 0*/) const
	{
		return GetSubresource(GfxSubresourceType::RTV, i);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::GetDSV(size_t i /*= 0*/) const
	{
		return GetSubresource(GfxSubresourceType::DSV, i);
	}

	uint32 GfxTexture::GetMappedRowPitch() const
	{
		return mapped_rowpitch;
	}

	uint64 GfxTexture::GetGPUAddress() const
	{
		return resource->GetGPUVirtualAddress();
	}

	ID3D12Resource* GfxTexture::GetNative() const
	{
		return resource.Get();
	}

	ID3D12Resource* GfxTexture::Detach()
	{
		return resource.Detach();
	}

	D3D12MA::Allocation* GfxTexture::DetachAllocation()
	{
		return allocation.release();
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
			BREAK_IF_FAILED(hr);
			mapped_rowpitch = static_cast<uint32_t>(footprint.Footprint.RowPitch);
		}
		else if (desc.heap_type == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range{};
			hr = resource->Map(0, &read_range, &mapped_data);
			BREAK_IF_FAILED(hr);
			mapped_rowpitch = static_cast<uint32>(footprint.Footprint.RowPitch);
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

	size_t GfxTexture::CreateSubresource(GfxSubresourceType view_type, GfxTextureSubresourceDesc const& view_desc)
	{
		GfxFormat format = GetDesc().format;
		ID3D12Device* device = gfx->GetDevice();

		switch (view_type)
		{
		case GfxSubresourceType::SRV:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				srv_desc.Format = ConvertGfxFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
					srv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					srv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					srv_desc.Texture1DArray.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture1DArray.MipLevels = view_desc.mip_count;
				}
				else
				{
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
					srv_desc.Texture1D.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture1D.MipLevels = view_desc.mip_count;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (HasAnyFlag(desc.misc_flags, GfxTextureMiscFlag::TextureCube))
					{
						if (desc.array_size > 6)
						{
							srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
							srv_desc.TextureCubeArray.First2DArrayFace = view_desc.first_slice;
							srv_desc.TextureCubeArray.NumCubes = std::min<uint32>(desc.array_size, view_desc.slice_count) / 6;
							srv_desc.TextureCubeArray.MostDetailedMip = view_desc.first_mip;
							srv_desc.TextureCubeArray.MipLevels = view_desc.mip_count;
						}
						else
						{
							srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
							srv_desc.TextureCube.MostDetailedMip = view_desc.first_mip;
							srv_desc.TextureCube.MipLevels = view_desc.mip_count;
						}
					}
					else
					{
						//if (texture->desc.sample_count > 1)
						//{
						//	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
						//	srv_desc.Texture2DMSArray.FirstArraySlice = firstSlice;
						//	srv_desc.Texture2DMSArray.ArraySize = sliceCount;
						//}
						//else
						srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
						srv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						srv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						srv_desc.Texture2DArray.MostDetailedMip = view_desc.first_mip;
						srv_desc.Texture2DArray.MipLevels = view_desc.mip_count;
					}
				}
				else
				{
					//if (texture->desc.sample_count > 1)
					//{
					//	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
					//}
					//else
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srv_desc.Texture2D.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture2D.MipLevels = view_desc.mip_count;
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				srv_desc.Texture3D.MostDetailedMip = view_desc.first_mip;
				srv_desc.Texture3D.MipLevels = view_desc.mip_count;
			}

			device->CreateShaderResourceView(resource.Get(), &srv_desc, descriptor);
			srvs.push_back(descriptor);
			return srvs.size() - 1;
		}
		break;
		case GfxSubresourceType::UAV:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				uav_desc.Format = ConvertGfxFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
					uav_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					uav_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					uav_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
					uav_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					uav_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
					uav_desc.Texture2DArray.ArraySize = view_desc.slice_count;
					uav_desc.Texture2DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					uav_desc.Texture2D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
				uav_desc.Texture3D.MipSlice = view_desc.first_mip;
				uav_desc.Texture3D.FirstWSlice = 0;
				uav_desc.Texture3D.WSize = -1;
			}

			device->CreateUnorderedAccessView(resource.Get(), nullptr, &uav_desc, descriptor);
			uavs.push_back(descriptor);
			return uavs.size() - 1;
		}
		break;
		case GfxSubresourceType::RTV:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				rtv_desc.Format = ConvertGfxFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
					rtv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					rtv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					rtv_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
					rtv_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (desc.sample_count > 1)
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
						rtv_desc.Texture2DMSArray.FirstArraySlice = view_desc.first_slice;
						rtv_desc.Texture2DMSArray.ArraySize = view_desc.slice_count;
					}
					else
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
						rtv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						rtv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						rtv_desc.Texture2DArray.MipSlice = view_desc.first_mip;
					}
				}
				else
				{
					if (desc.sample_count > 1)
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
						rtv_desc.Texture2D.MipSlice = view_desc.first_mip;
					}
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
				rtv_desc.Texture3D.MipSlice = view_desc.first_mip;
				rtv_desc.Texture3D.FirstWSlice = 0;
				rtv_desc.Texture3D.WSize = -1;
			}
			device->CreateRenderTargetView(resource.Get(), &rtv_desc, descriptor);
			rtvs.push_back(descriptor);
			return rtvs.size() - 1;
		}
		break;
		case GfxSubresourceType::DSV:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
				break;
			default:
				dsv_desc.Format = ConvertGfxFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
					dsv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					dsv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					dsv_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
					dsv_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (desc.sample_count > 1)
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
						dsv_desc.Texture2DMSArray.FirstArraySlice = view_desc.first_slice;
						dsv_desc.Texture2DMSArray.ArraySize = view_desc.slice_count;
					}
					else
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
						dsv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						dsv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						dsv_desc.Texture2DArray.MipSlice = view_desc.first_mip;
					}
				}
				else
				{
					if (desc.sample_count > 1)
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
						dsv_desc.Texture2D.MipSlice = view_desc.first_mip;
					}
				}
			}

			device->CreateDepthStencilView(resource.Get(), &dsv_desc, descriptor);
			dsvs.push_back(descriptor);
			return dsvs.size() - 1;
		}
		break;
		default:
			break;
		}
		return -1;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxTexture::GetSubresource(GfxSubresourceType type, size_t index /*= 0*/) const
	{
		switch (type)
		{
		case GfxSubresourceType::SRV:
			ADRIA_ASSERT(index < srvs.size());
			return srvs[index];
		case GfxSubresourceType::UAV:
			ADRIA_ASSERT(index < uavs.size());
			return uavs[index];
		case GfxSubresourceType::RTV:
			ADRIA_ASSERT(index < rtvs.size());
			return rtvs[index];
		case GfxSubresourceType::DSV:
			ADRIA_ASSERT(index < dsvs.size());
			return dsvs[index];
		default:
			ADRIA_ASSERT(false && "Invalid view type for texture!");
		}
		return { .ptr = NULL };
	}

}

