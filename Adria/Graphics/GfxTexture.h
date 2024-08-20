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
		GfxClearValue clear_value{};
		GfxResourceState initial_state = GfxResourceState::AllSRV;
		GfxFormat format = GfxFormat::UNKNOWN;

		std::strong_ordering operator<=>(GfxTextureDesc const& other) const = default;
		bool IsCompatible(GfxTextureDesc const& desc) const
		{
			return type == desc.type && width == desc.width && height == desc.height && array_size == desc.array_size
				&& format == desc.format && sample_count == desc.sample_count && heap_type == desc.heap_type
				&& HasAllFlags(bind_flags, desc.bind_flags) && HasAllFlags(misc_flags, desc.misc_flags) && clear_value == desc.clear_value;
		}
	};

	enum GfxTextureDescriptorFlags : uint8
	{
		GfxTextureDescriptorFlag_None = 0x0,
		GfxTextureDescriptorFlag_DepthReadOnly = 0x1
	};


	enum GfxTextureChannelMapping : uint32
	{
		GfxTextureChannelMapping_Red,
		GfxTextureChannelMapping_Green,
		GfxTextureChannelMapping_Blue,
		GfxTextureChannelMapping_Alpha,
		GfxTextureChannelMapping_Zero,
		GfxTextureChannelMapping_One,
	};

	static constexpr GfxTextureChannelMapping GfxCustomTextureChannelMapping(GfxTextureChannelMapping R, GfxTextureChannelMapping G,
		GfxTextureChannelMapping B, GfxTextureChannelMapping A)
	{
		constexpr uint32 GfxTextureChannelMappingMask = 0x7;
		constexpr uint32 GfxTextureChannelMappingShift = 3;
		constexpr uint32 GfxTextureChannelMappingAlwaysSetBit = 1 << (GfxTextureChannelMappingShift * 4);

		return GfxTextureChannelMapping(
			(((R) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 0)) |
			(((G) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 1)) |
			(((B) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 2)) |
			(((A) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 3)) |
			GfxTextureChannelMappingAlwaysSetBit);
	}

	inline constexpr GfxTextureChannelMapping GfxDefaultTextureChannelMapping = GfxCustomTextureChannelMapping(GfxTextureChannelMapping_Red, GfxTextureChannelMapping_Green,
																											   GfxTextureChannelMapping_Blue, GfxTextureChannelMapping_Alpha);
	inline constexpr GfxTextureChannelMapping GfxAlphaOneTextureChannelMapping = GfxCustomTextureChannelMapping(GfxTextureChannelMapping_Red, GfxTextureChannelMapping_Green,
																											    GfxTextureChannelMapping_Blue, GfxTextureChannelMapping_One);

	struct GfxTextureDescriptorDesc
	{
		uint32 first_slice = 0;
		uint32 slice_count = static_cast<uint32>(-1);
		uint32 first_mip = 0;
		uint32 mip_count = static_cast<uint32>(-1);

		GfxTextureDescriptorFlags flags = GfxTextureDescriptorFlag_None;
		GfxTextureChannelMapping channel_mapping = GfxDefaultTextureChannelMapping;
		std::strong_ordering operator<=>(GfxTextureDescriptorDesc const& other) const = default;
	};

	struct GfxTextureSubData
	{
		void const* data;
		uint64 row_pitch;
		uint64 slice_pitch;
	};

	struct GfxTextureData
	{
		GfxTextureSubData* sub_data = nullptr;
		uint32 sub_count = uint32(-1);
	};

	class GfxTexture
	{
	public:
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data);
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc);
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, void* backbuffer); //constructor used by swapchain for creating backbuffer texture
		ADRIA_NONCOPYABLE_NONMOVABLE(GfxTexture)
		~GfxTexture();

		ID3D12Resource* GetNative() const;

		GfxDevice* GetParent() const { return gfx; }
		GfxTextureDesc const& GetDesc() const { return desc; }
		uint32 GetWidth() const { return desc.width; }
		uint32 GetHeight() const { return desc.height; }
		uint32 GetDepth() const { return desc.depth; }
		GfxFormat GetFormat() const { return desc.format; }
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
		Ref<ID3D12Resource> resource;
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