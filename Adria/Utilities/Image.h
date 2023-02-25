#pragma once
#include <string_view>
#include <memory>
#include "../Core/Definitions.h"

namespace adria
{
	class Image
	{
	public:
		Image(std::string_view image_file, int32 desired_channels = 4);

		Image(Image const&) = delete;
		Image(Image&&) = default;
		Image& operator=(Image const&) = delete;
		Image& operator=(Image&&) = default;
		~Image() = default;

		uint32 Width() const
		{
			return _width;
		}
		uint32 Height() const
		{
			return _height;
		}
		uint32 Channels() const
		{
			return _channels;
		}
		uint32 BytesPerPixel() const
		{
			return _channels * (is_hdr ? sizeof(float) : sizeof(uint8));
		}
		uint32 Pitch() const
		{
			return _width * BytesPerPixel();
		}
		bool IsHDR() const
		{
			return is_hdr;
		}

		template<typename T>
		T const* Data() const;

	private:
		uint32 _width, _height;
		uint32 _channels;
		std::unique_ptr<uint8[]> _pixels;
		bool is_hdr;
	};

	template<typename T>
	T const* adria::Image::Data() const
	{
		return reinterpret_cast<T const*>(_pixels.get());
	}

}