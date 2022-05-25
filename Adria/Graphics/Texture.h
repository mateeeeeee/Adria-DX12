#pragma once
#include "ResourceCommon.h"

namespace adria
{
	enum ETextureType : uint8
	{
		TextureType_1D,
		TextureType_2D,
		TextureType_3D
	};
	
	struct TextureDesc
	{
		ETextureType type = TextureType_2D;
		uint32 width = 0;
		uint32 height = 0;
		uint32 depth = 0;
		uint32 array_size = 1;
		uint32 mip_levels = 1;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		uint32 sample_count = 1;
		EHeapType heap_type = EHeapType::Default;
		EBindFlag bind_flags = EBindFlag::None;
		EResourceMiscFlag misc_flags = EResourceMiscFlag::None;
		std::optional<D3D12_CLEAR_VALUE> clear = std::nullopt;
		D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		std::strong_ordering operator<=>(TextureDesc const& other) const = default;
	};

	struct TextureViewDesc
	{
		uint32 first_slice = 0;
		uint32 slice_count = static_cast<uint32>(-1);
		uint32 first_mip = 0;
		uint32 mip_count = static_cast<uint32>(-1);
		std::optional<DXGI_FORMAT> new_format = std::nullopt;

		std::strong_ordering operator<=>(TextureViewDesc const& other) const = default;
	};

	using TextureInitialData = D3D12_SUBRESOURCE_DATA;

	class Texture
	{
	public:
		Texture(GraphicsDevice* gfx, TextureDesc const& desc, TextureInitialData* initial_data = nullptr)
			: gfx(gfx), desc(desc)
		{
			HRESULT hr = E_FAIL;

			D3D12MA::ALLOCATION_DESC allocation_desc{};
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC resource_desc{};
			resource_desc.Format = desc.format;
			resource_desc.Width = desc.width;
			resource_desc.Height = desc.height;
			resource_desc.MipLevels = desc.mip_levels;
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resource_desc.DepthOrArraySize = (UINT16)desc.array_size;
			resource_desc.SampleDesc.Count = desc.sample_count;
			resource_desc.SampleDesc.Quality = 0;
			resource_desc.Alignment = 0;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			if (HasAllFlags(desc.bind_flags, EBindFlag::DepthStencil))
			{
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}
			if (!HasAllFlags(desc.bind_flags, EBindFlag::ShaderResource))
			{
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}
			if (HasAllFlags(desc.bind_flags, EBindFlag::RenderTarget))
			{
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			}
			if (HasAllFlags(desc.bind_flags, EBindFlag::UnorderedAccess))
			{
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			switch (desc.type)
			{
			case TextureType_1D:
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
				break;
			case TextureType_2D:
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				break;
			case TextureType_3D:
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
				resource_desc.DepthOrArraySize = (UINT16)desc.depth;
				break;
			default:
				ADRIA_ASSERT(false && "Invalid Texture Type!");
				break;
			}
			D3D12_CLEAR_VALUE* clear_value = nullptr;
			D3D12_CLEAR_VALUE clear = desc.clear.value_or(D3D12_CLEAR_VALUE{});
			if (desc.clear.has_value() && HasAnyFlag(desc.bind_flags, EBindFlag::RenderTarget | EBindFlag::DepthStencil))
			{
				clear_value = &clear;
			}

			D3D12_RESOURCE_STATES resource_state = desc.initial_state;
			if (initial_data != nullptr)
			{
				resource_state = D3D12_RESOURCE_STATE_COMMON;
			}

			auto device = gfx->GetDevice();
			if (desc.heap_type == EHeapType::Readback || desc.heap_type == EHeapType::Upload)
			{
				UINT64 RequiredSize = 0;
				device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &footprint, nullptr, nullptr, &RequiredSize);
				resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				resource_desc.Width = RequiredSize;
				resource_desc.Height = 1;
				resource_desc.DepthOrArraySize = 1;
				resource_desc.MipLevels = 1;
				resource_desc.Format = DXGI_FORMAT_UNKNOWN;
				resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

				if (desc.heap_type == EHeapType::Readback)
				{
					allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
					resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
				}
				else if (desc.heap_type == EHeapType::Upload)
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
				clear_value,
				&alloc,
				IID_PPV_ARGS(&resource)
			);
			BREAK_IF_FAILED(hr);
			allocation.reset(alloc);

			if (desc.heap_type == EHeapType::Readback)
			{
				hr = resource->Map(0, nullptr, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = footprint.Footprint.RowPitch;
			}
			else if (desc.heap_type == EHeapType::Upload)
			{
				D3D12_RANGE read_range = {};
				hr = resource->Map(0, &read_range, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = footprint.Footprint.RowPitch;
			}
			if (desc.mip_levels == 0)
			{
				const_cast<TextureDesc&>(desc).mip_levels = (uint32_t)log2(std::max<uint32>(desc.width, desc.height)) + 1;
			}

			auto upload_buffer = gfx->GetDynamicAllocator();
			auto cmd_list = gfx->GetDefaultCommandList();
			if (initial_data != nullptr)
			{
				uint32 data_count = desc.array_size * std::max<uint32>(1u, desc.mip_levels);
				UINT64 required_size;
				device->GetCopyableFootprints(&resource_desc, 0, data_count, 0, nullptr, nullptr, nullptr, &required_size);
				DynamicAllocation dyn_alloc = upload_buffer->Allocate(required_size);
				UpdateSubresources(cmd_list, resource.Get(), dyn_alloc.buffer,
					dyn_alloc.offset, 0, data_count, initial_data);
			}
		}

		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;
		~Texture()
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

		[[maybe_unused]] size_t CreateSRV(TextureViewDesc const* desc = nullptr)
		{
			TextureViewDesc _desc = desc ? *desc : TextureViewDesc{};
			return CreateView(EResourceViewType::SRV, _desc);
		}
		[[maybe_unused]] size_t CreateUAV(TextureViewDesc const* desc = nullptr)
		{
			TextureViewDesc _desc = desc ? *desc : TextureViewDesc{};
			return CreateView(EResourceViewType::UAV, _desc);
		}
		[[maybe_unused]] size_t CreateRTV(TextureViewDesc const* desc = nullptr)
		{
			TextureViewDesc _desc = desc ? *desc : TextureViewDesc{};
			return CreateView(EResourceViewType::RTV, _desc);
		}
		[[maybe_unused]] size_t CreateDSV(TextureViewDesc const* desc = nullptr)
		{
			TextureViewDesc _desc = desc ? *desc : TextureViewDesc{};
			return CreateView(EResourceViewType::DSV, _desc);
		}
		D3D12_CPU_DESCRIPTOR_HANDLE SRV(size_t i = 0) const { return GetView(EResourceViewType::SRV, i); }
		D3D12_CPU_DESCRIPTOR_HANDLE UAV(size_t i = 0) const { return GetView(EResourceViewType::UAV, i); }
		D3D12_CPU_DESCRIPTOR_HANDLE RTV(size_t i = 0) const { return GetView(EResourceViewType::RTV, i); }
		D3D12_CPU_DESCRIPTOR_HANDLE DSV(size_t i = 0) const { return GetView(EResourceViewType::DSV, i); }
		void ClearViews()
		{
			srvs.clear();
			uavs.clear();
			rtvs.clear();
			dsvs.clear();
		}

		bool IsMapped() const { return mapped_data != nullptr; }
		void* GetMappedData() const { return mapped_data; }
		template<typename T>
		T* GetMappedData() const { return reinterpret_cast<T*>(mapped_data); }
		void* Map()
		{
			HRESULT hr;
			if (desc.heap_type == EHeapType::Readback)
			{
				hr = resource->Map(0, nullptr, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = static_cast<uint32_t>(footprint.Footprint.RowPitch);
			}
			else if (desc.heap_type == EHeapType::Upload)
			{
				D3D12_RANGE read_range{};
				hr = resource->Map(0, &read_range, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = static_cast<uint32>(footprint.Footprint.RowPitch);
			}
			return mapped_data;
		}
		void Unmap()
		{
			resource->Unmap(0, nullptr);
		}

		uint32 GetMappedRowPitch() const { return mapped_rowpitch; }
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return resource->GetGPUVirtualAddress(); }
		ID3D12Resource* GetNative() const { return resource.Get(); }
		TextureDesc const& GetDesc() const { return desc; }

	private:
		GraphicsDevice* gfx;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		TextureDesc desc;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srvs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> uavs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsvs;

		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};

		void* mapped_data = nullptr;
		uint32 mapped_rowpitch = 0;

	private:

		[[maybe_unused]] size_t CreateView(EResourceViewType view_type, TextureViewDesc const& view_desc)
		{
			DXGI_FORMAT format = GetDesc().format;
			if (view_desc.new_format.has_value())
			{
				format = *view_desc.new_format;
			}
			Microsoft::WRL::ComPtr<ID3D12Device> device;
			resource->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

			switch (view_type)
			{
			case EResourceViewType::SRV:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
				srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				switch (format)
				{
				case DXGI_FORMAT_R16_TYPELESS:
					srv_desc.Format = DXGI_FORMAT_R16_UNORM;
					break;
				case DXGI_FORMAT_R32_TYPELESS:
					srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
					break;
				case DXGI_FORMAT_R24G8_TYPELESS:
					srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
					break;
				case DXGI_FORMAT_R32G8X24_TYPELESS:
					srv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
					break;
				default:
					srv_desc.Format = format;
					break;
				}

				if (desc.type == TextureType_1D)
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
				else if (desc.type == TextureType_2D)
				{
					if (desc.array_size > 1)
					{
						if (HasAnyFlag(desc.misc_flags, EResourceMiscFlag::TextureCube))
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
				else if (desc.type == TextureType_3D)
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
			case EResourceViewType::UAV:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
				switch (format)
				{
				case DXGI_FORMAT_R16_TYPELESS:
					uav_desc.Format = DXGI_FORMAT_R16_UNORM;
					break;
				case DXGI_FORMAT_R32_TYPELESS:
					uav_desc.Format = DXGI_FORMAT_R32_FLOAT;
					break;
				case DXGI_FORMAT_R24G8_TYPELESS:
					uav_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
					break;
				case DXGI_FORMAT_R32G8X24_TYPELESS:
					uav_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
					break;
				default:
					uav_desc.Format = format;
					break;
				}

				if (desc.type == TextureType_1D)
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
				else if (desc.type == TextureType_2D)
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
				else if (desc.type == TextureType_2D)
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
			case EResourceViewType::RTV:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
				switch (format)
				{
				case DXGI_FORMAT_R16_TYPELESS:
					rtv_desc.Format = DXGI_FORMAT_R16_UNORM;
					break;
				case DXGI_FORMAT_R32_TYPELESS:
					rtv_desc.Format = DXGI_FORMAT_R32_FLOAT;
					break;
				case DXGI_FORMAT_R24G8_TYPELESS:
					rtv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
					break;
				case DXGI_FORMAT_R32G8X24_TYPELESS:
					rtv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
					break;
				default:
					rtv_desc.Format = format;
					break;
				}

				if (desc.type == TextureType_1D)
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
				else if (desc.type == TextureType_2D)
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
				else if (desc.type == TextureType_3D)
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
			case EResourceViewType::DSV:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
				D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
				switch (format)
				{
				case DXGI_FORMAT_R16_TYPELESS:
					dsv_desc.Format = DXGI_FORMAT_D16_UNORM;
					break;
				case DXGI_FORMAT_R32_TYPELESS:
					dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
					break;
				case DXGI_FORMAT_R24G8_TYPELESS:
					dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					break;
				case DXGI_FORMAT_R32G8X24_TYPELESS:
					dsv_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
					break;
				default:
					dsv_desc.Format = format;
					break;
				}

				if (desc.type == TextureType_1D)
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
				else if (desc.type == TextureType_2D)
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
		D3D12_CPU_DESCRIPTOR_HANDLE GetView(EResourceViewType type, size_t index = 0) const
		{
			switch (type)
			{
			case EResourceViewType::SRV:
				ADRIA_ASSERT(index < srvs.size());
				return srvs[index];
			case EResourceViewType::UAV:
				ADRIA_ASSERT(index < uavs.size());
				return uavs[index];
			case EResourceViewType::RTV:
				ADRIA_ASSERT(index < rtvs.size());
				return rtvs[index];
			case EResourceViewType::DSV:
				ADRIA_ASSERT(index < dsvs.size());
				return dsvs[index];
			default:
				ADRIA_ASSERT(false && "Invalid view type for buffer!");
			}
			return { .ptr = NULL };
		}

	};
}