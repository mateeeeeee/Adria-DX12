#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "ImageWrite.h"

namespace adria
{

	void WriteImageToFile(FileType type, std::string_view filename, uint32 width, uint32 height, void const* data, uint32 stride)
	{
		switch (type)
		{
		case FileType::PNG: stbi_write_png(filename.data(), (int)width, (int)height, 4, data, (int)stride); break;
		case FileType::JPG: stbi_write_jpg(filename.data(), (int)width, (int)height, 4, data, 100); break;
		case FileType::HDR: stbi_write_hdr(filename.data(), (int)width, (int)height, 4, (float*)data); break;
		case FileType::TGA: stbi_write_tga(filename.data(), (int)width, (int)height, 4, data); break;
		case FileType::BMP: stbi_write_bmp(filename.data(), (int)width, (int)height, 4, data); break;
		default: ADRIA_UNREACHABLE();
		}
	}
}

