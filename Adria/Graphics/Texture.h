#pragma once
#include "ResourceCommon.h"

namespace adria
{	
	struct ClearValue
	{
		enum class ActiveMember
		{
			None,
			Color,
			DepthStencil
		};
		struct ClearColor
		{
			ClearColor(float32 r = 0.0f, float32 g = 0.0f, float32 b = 0.0f, float32 a = 0.0f)
				: color{ r, g, b, a }
			{
			}
			ClearColor(float32(&_color)[4])
				: color{ _color[0], _color[1], _color[2], _color[3] }
			{
			}
			ClearColor(ClearColor const& other)
				: color{ other.color[0], other.color[1], other.color[2], other.color[3] }
			{
			}

			bool operator==(ClearColor const& other) const
			{
				return memcmp(color, other.color, sizeof(color)) == 0;
			}

			float32 color[4];
		};
		struct ClearDepthStencil
		{
			ClearDepthStencil(float32 depth = 0.0f, uint8 stencil = 1)
				: depth(depth), stencil(stencil)
			{}
			float32 depth;
			uint8 stencil;
		};

		ClearValue() : active_member(ActiveMember::None), depth_stencil{} {}

		ClearValue(float32 r, float32 g, float32 b, float32 a)
			: active_member(ActiveMember::Color), color(r, g, b, a)
		{
		}

		ClearValue(float32(&_color)[4])
			: active_member(ActiveMember::Color), color{_color }
		{
		}

		ClearValue(ClearColor const& color)
			: active_member(ActiveMember::Color), color(color)
		{}

		ClearValue(float32 depth, uint8 stencil)
			: active_member(ActiveMember::DepthStencil), depth_stencil(depth, stencil)
		{}
		ClearValue(ClearDepthStencil const& depth_stencil)
			: active_member(ActiveMember::DepthStencil), depth_stencil(depth_stencil)
		{}

		ClearValue(ClearValue const& other)
			: active_member(other.active_member), color{}
		{
			if (active_member == ActiveMember::Color) color = other.color;
			else if (active_member == ActiveMember::DepthStencil) depth_stencil = other.depth_stencil;
		}

		bool operator==(ClearValue const& other) const
		{
			if (active_member != other.active_member) return false;
			else if (active_member == ActiveMember::Color)
			{
				return color == other.color;
			}
			else return depth_stencil.depth == other.depth_stencil.depth
				&& depth_stencil.stencil == other.depth_stencil.stencil;
		}

		ActiveMember active_member;
		union
		{
			ClearColor color;
			ClearDepthStencil depth_stencil;
		};
	};

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
		uint32 sample_count = 1;
		EResourceUsage heap_type = EResourceUsage::Default;
		EBindFlag bind_flags = EBindFlag::None;
		ETextureMiscFlag misc_flags = ETextureMiscFlag::None;
		EResourceState initial_state = EResourceState::PixelShaderResource | EResourceState::NonPixelShaderResource;//D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		ClearValue clear_value{};
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

		std::strong_ordering operator<=>(TextureDesc const& other) const = default;
		bool IsCompatible(TextureDesc const& desc) const
		{
			return type == desc.type && width == desc.width && height == desc.height && array_size == desc.array_size
				&& format == desc.format && sample_count == desc.sample_count && heap_type == desc.heap_type
				&& HasAllFlags(bind_flags, desc.bind_flags) && HasAllFlags(misc_flags, desc.misc_flags);
		}
	};

	struct TextureSubresourceDesc
	{
		uint32 first_slice = 0;
		uint32 slice_count = static_cast<uint32>(-1);
		uint32 first_mip = 0;
		uint32 mip_count = static_cast<uint32>(-1);

		std::strong_ordering operator<=>(TextureSubresourceDesc const& other) const = default;
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

				if (!HasAllFlags(desc.bind_flags, EBindFlag::ShaderResource))
				{
					resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
				}
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
			D3D12_CLEAR_VALUE* clear_value_ptr = nullptr;
			D3D12_CLEAR_VALUE clear_value{};
			if (HasAnyFlag(desc.bind_flags, EBindFlag::DepthStencil) && desc.clear_value.active_member == ClearValue::ActiveMember::DepthStencil)
			{
				clear_value.DepthStencil.Depth = desc.clear_value.depth_stencil.depth;
				clear_value.DepthStencil.Stencil = desc.clear_value.depth_stencil.stencil;
				switch (desc.format)
				{
				case DXGI_FORMAT_R16_TYPELESS:
					clear_value.Format = DXGI_FORMAT_D16_UNORM;
					break;
				case DXGI_FORMAT_R32_TYPELESS:
					clear_value.Format = DXGI_FORMAT_D32_FLOAT;
					break;
				case DXGI_FORMAT_R24G8_TYPELESS:
					clear_value.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					break;
				case DXGI_FORMAT_R32G8X24_TYPELESS:
					clear_value.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
					break;
				default:
					clear_value.Format = desc.format;
					break;
				}
				clear_value_ptr = &clear_value;
			}
			else if (HasAnyFlag(desc.bind_flags, EBindFlag::RenderTarget) && desc.clear_value.active_member == ClearValue::ActiveMember::Color)
			{
				clear_value.Color[0] = desc.clear_value.color.color[0];
				clear_value.Color[1] = desc.clear_value.color.color[1];
				clear_value.Color[2] = desc.clear_value.color.color[2];
				clear_value.Color[3] = desc.clear_value.color.color[3];
				switch (desc.format)
				{
				case DXGI_FORMAT_R16_TYPELESS:
					clear_value.Format = DXGI_FORMAT_R16_UNORM;
					break;
				case DXGI_FORMAT_R32_TYPELESS:
					clear_value.Format = DXGI_FORMAT_R32_FLOAT;
					break;
				case DXGI_FORMAT_R24G8_TYPELESS:
					clear_value.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
					break;
				case DXGI_FORMAT_R32G8X24_TYPELESS:
					clear_value.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
					break;
				default:
					clear_value.Format = desc.format;
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
			if (desc.heap_type == EResourceUsage::Readback || desc.heap_type == EResourceUsage::Upload)
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

				if (desc.heap_type == EResourceUsage::Readback)
				{
					allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
					resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
				}
				else if (desc.heap_type == EResourceUsage::Upload)
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
				IID_PPV_ARGS(&resource)
			);
			BREAK_IF_FAILED(hr);
			allocation.reset(alloc);

			if (desc.heap_type == EResourceUsage::Readback)
			{
				hr = resource->Map(0, nullptr, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = footprint.Footprint.RowPitch;
			}
			else if (desc.heap_type == EResourceUsage::Upload)
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
				DynamicAllocation dyn_alloc = upload_buffer->Allocate(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
				UpdateSubresources(cmd_list, resource.Get(), dyn_alloc.buffer,
					dyn_alloc.offset, 0, data_count, initial_data);

				if (desc.initial_state != EResourceState::CopyDest)
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

		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;
		Texture(Texture&&) = delete;
		Texture& operator=(Texture&&) = delete;
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

		[[maybe_unused]] size_t CreateSubresource_SRV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			return CreateSubresource(SubresourceType_SRV, _desc);
		}
		[[maybe_unused]] size_t CreateSubresource_UAV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			return CreateSubresource(SubresourceType_UAV, _desc);
		}
		[[maybe_unused]] size_t CreateSubresource_RTV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			return CreateSubresource(SubresourceType_RTV, _desc);
		}
		[[maybe_unused]] size_t CreateSubresource_DSV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			return CreateSubresource(SubresourceType_DSV, _desc);
		}
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeSubresource_SRV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			size_t i = CreateSubresource(SubresourceType_SRV, _desc);
			ADRIA_ASSERT(srvs.size() - 1 == i);
			D3D12_CPU_DESCRIPTOR_HANDLE srv = srvs.back();
			srvs.pop_back();
			return srv;
		}
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeSubresource_UAV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			size_t i = CreateSubresource(SubresourceType_UAV, _desc);
			ADRIA_ASSERT(uavs.size() - 1 == i);
			D3D12_CPU_DESCRIPTOR_HANDLE uav = uavs.back();
			uavs.pop_back();
			return uav;
		}
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeSubresource_RTV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			size_t i = CreateSubresource(SubresourceType_RTV, _desc);
			ADRIA_ASSERT(rtvs.size() - 1 == i);
			D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvs.back();
			rtvs.pop_back();
			return rtv;
		}
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeSubresource_DSV(TextureSubresourceDesc const* desc = nullptr)
		{
			TextureSubresourceDesc _desc = desc ? *desc : TextureSubresourceDesc{};
			size_t i = CreateSubresource(SubresourceType_DSV, _desc);
			ADRIA_ASSERT(dsvs.size() - 1 == i);
			D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvs.back();
			dsvs.pop_back();
			return dsv;
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource_SRV(size_t i = 0) const { return GetSubresource(SubresourceType_SRV, i); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource_UAV(size_t i = 0) const { return GetSubresource(SubresourceType_UAV, i); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource_RTV(size_t i = 0) const { return GetSubresource(SubresourceType_RTV, i); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource_DSV(size_t i = 0) const { return GetSubresource(SubresourceType_DSV, i); }

		uint32 GetMappedRowPitch() const { return mapped_rowpitch; }
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return resource->GetGPUVirtualAddress(); }
		ID3D12Resource* GetNative() const { return resource.Get(); }
		TextureDesc const& GetDesc() const { return desc; }

		bool IsMapped() const { return mapped_data != nullptr; }
		void* GetMappedData() const { return mapped_data; }
		template<typename T>
		T* GetMappedData() const { return reinterpret_cast<T*>(mapped_data); }
		void* Map()
		{
			HRESULT hr;
			if (desc.heap_type == EResourceUsage::Readback)
			{
				hr = resource->Map(0, nullptr, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = static_cast<uint32_t>(footprint.Footprint.RowPitch);
			}
			else if (desc.heap_type == EResourceUsage::Upload)
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

		void SetName(char const* name)
		{
			resource->SetName(ConvertToWide(name).c_str());
		}
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

		[[maybe_unused]] size_t CreateSubresource(ESubresourceType view_type, TextureSubresourceDesc const& view_desc)
		{
			DXGI_FORMAT format = GetDesc().format;
			Microsoft::WRL::ComPtr<ID3D12Device> device;
			resource->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

			switch (view_type)
			{
			case SubresourceType_SRV:
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
						if (HasAnyFlag(desc.misc_flags, ETextureMiscFlag::TextureCube))
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
			case SubresourceType_UAV:
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
			case SubresourceType_RTV:
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
			case SubresourceType_DSV:
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
		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource(ESubresourceType type, size_t index = 0) const
		{
			switch (type)
			{
			case SubresourceType_SRV:
				ADRIA_ASSERT(index < srvs.size());
				return srvs[index];
			case SubresourceType_UAV:
				ADRIA_ASSERT(index < uavs.size());
				return uavs[index];
			case SubresourceType_RTV:
				ADRIA_ASSERT(index < rtvs.size());
				return rtvs[index];
			case SubresourceType_DSV:
				ADRIA_ASSERT(index < dsvs.size());
				return dsvs[index];
			default:
				ADRIA_ASSERT(false && "Invalid view type for buffer!");
			}
			return { .ptr = NULL };
		}

	};
}