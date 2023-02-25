#include "Image.h"
#include <stb_image.h>
#include "../Logging/Logger.h"

namespace adria
{

	Image::Image(std::string_view image_file, int32 desired_channels /*= 4*/)
	{
		int32 width, height, channels;

		if (is_hdr = static_cast<bool>(stbi_is_hdr(image_file.data())); is_hdr)
		{
			float* pixels = stbi_loadf(image_file.data(), &width, &height, &channels, desired_channels);
			if (!pixels)
			{
				ADRIA_LOG(ERROR, "Loading Image File %s unsuccessful", image_file.data());
			}
			else  _pixels.reset(reinterpret_cast<uint8*>(pixels));
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

		_width = static_cast<uint32>(width);
		_height = static_cast<uint32>(height);
		_channels = static_cast<uint32>(desired_channels);
	}

}