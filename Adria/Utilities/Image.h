#pragma once
#include <string_view>
#include <memory>
#include <stb_image.h>
#include "../Logging/Logger.h"
#include "../Core/Definitions.h"

namespace adria
{
	class Image
	{
	public:
		Image(std::string_view image_file, i32 desired_channels = 4)
		{
			i32 width, height, channels;

			if (is_hdr = static_cast<bool>(stbi_is_hdr(image_file.data())); is_hdr)
			{
				f32* pixels = stbi_loadf(image_file.data(), &width, &height, &channels, desired_channels);
				if (!pixels) GLOBAL_LOG_ERROR("Loading Image File " + std::string(image_file.data()) + " Unsuccessful");
				else  _pixels.reset(reinterpret_cast<u8*>(pixels));
			}
			else
			{
				stbi_uc* pixels = stbi_load(image_file.data(), &width, &height, &channels, desired_channels);
				if (!pixels) GLOBAL_LOG_ERROR("Loading Image File " + std::string(image_file.data()) + " Unsuccessful");
				else _pixels.reset(pixels);
			}

			_width = static_cast<u32>(width);
			_height = static_cast<u32>(height);
			_channels = static_cast<u32>(desired_channels);
		}

		Image(Image const&) = delete;
		Image(Image&&) = default;
		Image& operator=(Image const&) = delete;
		Image& operator=(Image&&) = default;
		~Image() = default;


		u32 Width() const
		{
			return _width;
		}

		u32 Height() const
		{
			return _height;
		}

		u32 Channels() const
		{
			return _channels;
		}

		u32 BytesPerPixel() const
		{
			return _channels * (is_hdr ? sizeof(f32) : sizeof(u8));
		}

		u32 Pitch() const
		{
			return _width * BytesPerPixel();
		}

		bool IsHDR() const
		{
			return is_hdr;
		}

		template<typename T>
		T const* Data() const
		{
			return reinterpret_cast<T const*>(_pixels.get());
		}


	private:
		u32 _width, _height;
		u32 _channels;
		std::unique_ptr<u8[]> _pixels;
		bool is_hdr;
	};


}