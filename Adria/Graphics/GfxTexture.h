#pragma once
#include "GfxResourceCommon.h"

namespace adria
{	
	struct GfxClearValue
	{
		enum class GfxActiveMember
		{
			None,
			Color,
			DepthStencil
		};
		struct GfxClearColor
		{
			GfxClearColor(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 0.0f)
				: color{ r, g, b, a }
			{
			}
			GfxClearColor(float(&_color)[4])
				: color{ _color[0], _color[1], _color[2], _color[3] }
			{
			}
			GfxClearColor(GfxClearColor const& other)
				: color{ other.color[0], other.color[1], other.color[2], other.color[3] }
			{
			}

			bool operator==(GfxClearColor const& other) const
			{
				return memcmp(color, other.color, sizeof(color)) == 0;
			}

			float color[4];
		};
		struct GfxClearDepthStencil
		{
			GfxClearDepthStencil(float depth = 0.0f, uint8 stencil = 1)
				: depth(depth), stencil(stencil)
			{}
			float depth;
			uint8 stencil;
		};

		GfxClearValue() : active_member(GfxActiveMember::None), depth_stencil{} {}

		GfxClearValue(float r, float g, float b, float a)
			: active_member(GfxActiveMember::Color), color(r, g, b, a)
		{
		}

		GfxClearValue(float(&_color)[4])
			: active_member(GfxActiveMember::Color), color{_color }
		{
		}

		GfxClearValue(GfxClearColor const& color)
			: active_member(GfxActiveMember::Color), color(color)
		{}

		GfxClearValue(float depth, uint8 stencil)
			: active_member(GfxActiveMember::DepthStencil), depth_stencil(depth, stencil)
		{}
		GfxClearValue(GfxClearDepthStencil const& depth_stencil)
			: active_member(GfxActiveMember::DepthStencil), depth_stencil(depth_stencil)
		{}

		GfxClearValue(GfxClearValue const& other)
			: active_member(other.active_member), color{}
		{
			if (active_member == GfxActiveMember::Color) color = other.color;
			else if (active_member == GfxActiveMember::DepthStencil) depth_stencil = other.depth_stencil;
		}

		bool operator==(GfxClearValue const& other) const
		{
			if (active_member != other.active_member) return false;
			else if (active_member == GfxActiveMember::Color)
			{
				return color == other.color;
			}
			else return depth_stencil.depth == other.depth_stencil.depth
				&& depth_stencil.stencil == other.depth_stencil.stencil;
		}

		GfxActiveMember active_member;
		union
		{
			GfxClearColor color;
			GfxClearDepthStencil depth_stencil;
		};
	};

	enum GfxTextureType : uint8
	{
		GfxTextureType_1D,
		GfxTextureType_2D,
		GfxTextureType_3D
	};
	inline constexpr GfxTextureType ConvertTextureType(D3D12_RESOURCE_DIMENSION dimension)
	{
		switch (dimension)
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			return GfxTextureType_1D;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			return GfxTextureType_2D;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			return GfxTextureType_3D;
		case D3D12_RESOURCE_DIMENSION_BUFFER:
		case D3D12_RESOURCE_DIMENSION_UNKNOWN:
		default:
			ADRIA_UNREACHABLE();
			return GfxTextureType_1D;
		}
		return GfxTextureType_1D;
	}
	
	struct GfxTextureDesc
	{
		GfxTextureType type = GfxTextureType_2D;
		uint32 width = 0;
		uint32 height = 0;
		uint32 depth = 0;
		uint32 array_size = 1;
		uint32 mip_levels = 1;
		uint32 sample_count = 1;
		GfxResourceUsage heap_type = GfxResourceUsage::Default;
		GfxBindFlag bind_flags = GfxBindFlag::None;
		GfxTextureMiscFlag misc_flags = GfxTextureMiscFlag::None;
		GfxResourceState initial_state = GfxResourceState::PixelShaderResource | GfxResourceState::NonPixelShaderResource;
		GfxClearValue clear_value{};
		GfxFormat format = GfxFormat::UNKNOWN;

		std::strong_ordering operator<=>(GfxTextureDesc const& other) const = default;
		bool IsCompatible(GfxTextureDesc const& desc) const
		{
			return type == desc.type && width == desc.width && height == desc.height && array_size == desc.array_size
				&& format == desc.format && sample_count == desc.sample_count && heap_type == desc.heap_type
				&& HasAllFlags(bind_flags, desc.bind_flags) && HasAllFlags(misc_flags, desc.misc_flags);
		}
	};
	struct GfxTextureSubresourceDesc
	{
		uint32 first_slice = 0;
		uint32 slice_count = static_cast<uint32>(-1);
		uint32 first_mip = 0;
		uint32 mip_count = static_cast<uint32>(-1);

		std::strong_ordering operator<=>(GfxTextureSubresourceDesc const& other) const = default;
	};
	using GfxTextureInitialData = D3D12_SUBRESOURCE_DATA;

	class GfxTexture
	{
	public:
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureInitialData* initial_data = nullptr, size_t data_count = -1);

		GfxTexture(GfxTexture const&) = delete;
		GfxTexture& operator=(GfxTexture const&) = delete;
		GfxTexture(GfxTexture&&) = delete;
		GfxTexture& operator=(GfxTexture&&) = delete;
		~GfxTexture();

		[[maybe_unused]] size_t CreateSRV(GfxTextureSubresourceDesc const* desc = nullptr);
		[[maybe_unused]] size_t CreateUAV(GfxTextureSubresourceDesc const* desc = nullptr);
		[[maybe_unused]] size_t CreateRTV(GfxTextureSubresourceDesc const* desc = nullptr);
		[[maybe_unused]] size_t CreateDSV(GfxTextureSubresourceDesc const* desc = nullptr);
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeSRV(GfxTextureSubresourceDesc const* desc = nullptr);
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeUAV(GfxTextureSubresourceDesc const* desc = nullptr);
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeRTV(GfxTextureSubresourceDesc const* desc = nullptr);
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeDSV(GfxTextureSubresourceDesc const* desc = nullptr);
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(size_t i = 0) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(size_t i = 0) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(size_t i = 0) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(size_t i = 0) const;

		uint32 GetMappedRowPitch() const;
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const;
		ID3D12Resource* GetNative() const;
		ID3D12Resource* Detach();
		D3D12MA::Allocation* DetachAllocation();
		GfxTextureDesc const& GetDesc() const;

		bool IsMapped() const;
		void* GetMappedData() const;
		
		template<typename T>
		T* GetMappedData() const;
		
		void* Map();
		void Unmap();

		void SetName(char const* name);
	private:
		GfxDevice* gfx;
		ArcPtr<ID3D12Resource> resource;
		GfxTextureDesc desc;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srvs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> uavs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsvs;

		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};

		void* mapped_data = nullptr;
		uint32 mapped_rowpitch = 0;

	private:

		[[maybe_unused]] size_t CreateSubresource(GfxSubresourceType view_type, GfxTextureSubresourceDesc const& view_desc);
		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource(GfxSubresourceType type, size_t index = 0) const;
	};

	template<typename T>
	T* GfxTexture::GetMappedData() const
	{
		return reinterpret_cast<T*>(mapped_data);
	}

}