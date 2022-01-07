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
		Image(std::string_view image_file, I32 desired_channels = 4)
		{
			I32 width, height, channels;

			if (is_hdr = static_cast<bool>(stbi_is_hdr(image_file.data())); is_hdr)
			{
				F32* pixels = stbi_loadf(image_file.data(), &width, &height, &channels, desired_channels);
				if (!pixels)
				{
					ADRIA_LOG(ERROR, "Loading Image File %s unsuccessful", image_file.data());
				}
				else  _pixels.reset(reinterpret_cast<U8*>(pixels));
			}
			else
			{
				stbi_uc* pixels = stbi_load(image_file.data(), &width, &height, &channels, desired_channels);
				if (!pixels)
				{
					ADRIA_LOG(ERROR, "Loading Image File %s unsuccessful", image_file.data());
				}
				else _pixels.reset(pixels);
			}

			_width = static_cast<U32>(width);
			_height = static_cast<U32>(height);
			_channels = static_cast<U32>(desired_channels);
		}

		Image(Image const&) = delete;
		Image(Image&&) = default;
		Image& operator=(Image const&) = delete;
		Image& operator=(Image&&) = default;
		~Image() = default;


		U32 Width() const
		{
			return _width;
		}

		U32 Height() const
		{
			return _height;
		}

		U32 Channels() const
		{
			return _channels;
		}

		U32 BytesPerPixel() const
		{
			return _channels * (is_hdr ? sizeof(F32) : sizeof(U8));
		}

		U32 Pitch() const
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
		U32 _width, _height;
		U32 _channels;
		std::unique_ptr<U8[]> _pixels;
		bool is_hdr;
	};


}