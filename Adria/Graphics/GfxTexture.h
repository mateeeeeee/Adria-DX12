#pragma once
#include "GfxResourceCommon.h"

namespace adria
{
	enum GfxTextureType : uint8
	{
		GfxTextureType_1D,
		GfxTextureType_2D,
		GfxTextureType_3D
	};
	inline char const* GfxTextureTypeToString(GfxTextureType type)
	{
		switch (type)
		{
		case GfxTextureType_1D: return "Texture 1D";
		case GfxTextureType_2D: return "Texture 2D";
		case GfxTextureType_3D: return "Texture 3D";
		default: return "Invalid";
		}
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
				&& HasAllFlags(bind_flags, desc.bind_flags) && HasAllFlags(misc_flags, desc.misc_flags) && clear_value == desc.clear_value;
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

	struct GfxTextureInitialData
	{
		void const* data;
		uint64 row_pitch;
		uint64 slice_pitch;
	};

	class GfxTexture
	{
	public:
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureInitialData* initial_data, uint32 subresource_count);
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureInitialData* initial_data = nullptr);
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, void* backbuffer); //constructor used by swapchain for creating backbuffer texture

		GfxTexture(GfxTexture const&) = delete;
		GfxTexture& operator=(GfxTexture const&) = delete;
		GfxTexture(GfxTexture&&) = delete;
		GfxTexture& operator=(GfxTexture&&) = delete;
		~GfxTexture();

		ID3D12Resource* GetNative() const;

		GfxDevice* GetParent() const { return gfx; }
		GfxTextureDesc const& GetDesc() const;
		uint64 GetGpuAddress() const;

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
		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		void* mapped_data = nullptr;
		bool is_backbuffer = false;
	};

	template<typename T>
	T* GfxTexture::GetMappedData() const
	{
		return reinterpret_cast<T*>(mapped_data);
	}
}