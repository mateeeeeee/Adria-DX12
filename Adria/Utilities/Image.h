#pragma once
#include <string_view>
#include <memory>
#include "Graphics/GfxFormat.h"

namespace adria
{
	class Image
	{
	public:
		explicit Image(GfxFormat format) : format(format) {}
		explicit Image(std::string_view file_path);

		uint32 Width() const
		{
			return width;
		}
		uint32 Height() const
		{
			return height;
		}
		uint32 Depth() const
		{
			return depth;
		}
		uint32 MipLevels() const
		{
			return mip_levels;
		}
		GfxFormat Format() const { return format; }
		bool IsHDR() const { return is_hdr; }
		bool IsCubemap() const { return is_cubemap; }

		template<typename T = uint8>
		T const* Data() const;
		template<typename T = uint8>
		T const* MipData(uint32 mip_level) const;

		Image const* NextImage() const { return next_image.get(); }

	private:
		uint32 width = 0;
		uint32 height = 0;
		uint32 depth = 0;
		uint32 mip_levels = 0;
		std::vector<uint8> pixels;
		bool is_hdr = false;
		bool is_cubemap = false;
		bool is_srgb = false;
		GfxFormat format = GfxFormat::UNKNOWN;
		std::unique_ptr<Image> next_image = nullptr;

	private:
		uint64 SetData(uint32 width, uint32 height, uint32 depth, uint32 mip_levels, void const* data);

		bool LoadDDS(std::string_view texture_path);
		bool LoadSTB(std::string_view texture_path);
	};

	template<typename T>
	T const* Image::Data() const
	{
		return reinterpret_cast<T const*>(pixels.data());
	}

	template<typename T>
	T const* Image::MipData(uint32 mip_level) const
	{
		uint64 offset = 0;
		for (uint32 mip = 0; mip < mip_level; ++mip)
		{
			offset += GetTextureMipByteSize(format, width, height, depth, mip);
		}
		return reinterpret_cast<T const*>(pixels.data() + offset);
	}
}