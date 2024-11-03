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

		Uint32 Width() const
		{
			return width;
		}
		Uint32 Height() const
		{
			return height;
		}
		Uint32 Depth() const
		{
			return depth;
		}
		Uint32 MipLevels() const
		{
			return mip_levels;
		}
		GfxFormat Format() const { return format; }
		Bool IsHDR() const { return is_hdr; }
		Bool IsCubemap() const { return is_cubemap; }

		template<typename T = Uint8>
		T const* Data() const;
		template<typename T = Uint8>
		T const* MipData(Uint32 mip_level) const;

		Image const* NextImage() const { return next_image.get(); }

	private:
		Uint32 width = 0;
		Uint32 height = 0;
		Uint32 depth = 0;
		Uint32 mip_levels = 0;
		std::vector<Uint8> pixels;
		Bool is_hdr = false;
		Bool is_cubemap = false;
		Bool is_srgb = false;
		GfxFormat format = GfxFormat::UNKNOWN;
		std::unique_ptr<Image> next_image = nullptr;

	private:
		Uint64 SetData(Uint32 width, Uint32 height, Uint32 depth, Uint32 mip_levels, void const* data);

		Bool LoadDDS(std::string_view texture_path);
		Bool LoadSTB(std::string_view texture_path);
	};

	template<typename T>
	T const* Image::Data() const
	{
		return reinterpret_cast<T const*>(pixels.data());
	}

	template<typename T>
	T const* Image::MipData(Uint32 mip_level) const
	{
		Uint64 offset = 0;
		for (Uint32 mip = 0; mip < mip_level; ++mip)
		{
			offset += GetTextureMipByteSize(format, width, height, depth, mip);
		}
		return reinterpret_cast<T const*>(pixels.data() + offset);
	}
}