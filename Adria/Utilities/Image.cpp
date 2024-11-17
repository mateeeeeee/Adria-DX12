#include "Image.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Logging/Logger.h"
#include "Utilities/FilesUtil.h"
#include "Utilities/StringUtil.h"

namespace adria
{

	namespace
	{
		enum class ImageFormat
		{
			DDS,
			BMP,
			JPG,
			PNG,
			TIFF,
			GIF,
			TGA,
			HDR,
			NotSupported
		};
		ImageFormat GetImageFormat(std::string_view path)
		{
			std::string extension = GetExtension(path);
			std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](Char c) {return std::tolower(c); });

			if (extension == ".dds")
				return ImageFormat::DDS;
			else if (extension == ".bmp")
				return ImageFormat::BMP;
			else if (extension == ".jpg" || extension == ".jpeg")
				return ImageFormat::JPG;
			else if (extension == ".png")
				return ImageFormat::PNG;
			else if (extension == ".tiff" || extension == ".tif")
				return ImageFormat::TIFF;
			else if (extension == ".gif")
				return ImageFormat::GIF;
			else if (extension == ".tga")
				return ImageFormat::TGA;
			else if (extension == ".hdr")
				return ImageFormat::HDR;
			else
				return ImageFormat::NotSupported;
		}
		ImageFormat GetImageFormat(std::wstring_view path)
		{
			return GetImageFormat(ToString(std::wstring(path)));
		}
	}

	Image::Image(std::string_view file_path)
	{
		ImageFormat format = GetImageFormat(file_path);
		Bool result;
		switch (format)
		{
		case ImageFormat::DDS:
			result = LoadDDS(file_path);
			break;
		case ImageFormat::BMP:
		case ImageFormat::PNG:
		case ImageFormat::JPG:
		case ImageFormat::TGA:
		case ImageFormat::TIFF:
		case ImageFormat::GIF:
		case ImageFormat::HDR:
			result = LoadSTB(file_path);
			break;
		case ImageFormat::NotSupported:
		default:
			ADRIA_ASSERT_MSG(false, "Unsupported Texture Format!");
		}
		ADRIA_ASSERT(result);
	}

	Uint64 Image::SetData(Uint32 _width, Uint32 _height, Uint32 _depth, Uint32 _mip_levels, void const* _data)
	{
		width = std::max(_width, 1u);
		height = std::max(_height, 1u);
		depth = std::max(_depth, 1u);
		mip_levels = std::max(_mip_levels, 1u);
		Uint64 texture_byte_size = GetTextureByteSize(format, width, height, depth, mip_levels);
		pixels.resize(texture_byte_size);
		memcpy(pixels.data(), _data, pixels.size());
		return texture_byte_size;
	}

	Bool Image::LoadDDS(std::string_view texture_path)
	{
		//https://github.com/simco50/D3D12_Research/blob/master/D3D12/Content/Image.cpp - LoadDDS

		FILE* file = nullptr;
		fopen_s(&file, texture_path.data(), "rb");
		if (!file)
			return false;

		fseek(file, 0, SEEK_END);
		std::vector<Char> data((Uint64)ftell(file));
		fseek(file, 0, SEEK_SET);
		fread(data.data(), data.size(), 1, file);

		Char* bytes = data.data();
#pragma pack(push,1)
		struct PixelFormatHeader
		{
			Uint32 dwSize;
			Uint32 dwFlags;
			Uint32 dwFourCC;
			Uint32 dwRGBBitCount;
			Uint32 dwRBitMask;
			Uint32 dwGBitMask;
			Uint32 dwBBitMask;
			Uint32 dwABitMask;
		};
#pragma pack(pop)

		// .DDS header.
#pragma pack(push,1)
		struct FileHeader
		{
			Uint32 dwSize;
			Uint32 dwFlags;
			Uint32 dwHeight;
			Uint32 dwWidth;
			Uint32 dwLinearSize;
			Uint32 dwDepth;
			Uint32 dwMipMapCount;
			Uint32 dwReserved1[11];
			PixelFormatHeader ddpf;
			Uint32 dwCaps;
			Uint32 dwCaps2;
			Uint32 dwCaps3;
			Uint32 dwCaps4;
			Uint32 dwReserved2;
		};
#pragma pack(pop)

		// .DDS 10 header.
#pragma pack(push,1)
		struct DX10FileHeader
		{
			Uint32 dxgiFormat;
			Uint32 resourceDimension;
			Uint32 miscFlag;
			Uint32 arraySize;
			Uint32 reserved;
		};
#pragma pack(pop)

		enum DDS_CAP_ATTRIBUTE
		{
			DDSCAPS_COMPLEX = 0x00000008U,
			DDSCAPS_TEXTURE = 0x00001000U,
			DDSCAPS_MIPMAP = 0x00400000U,
			DDSCAPS2_VOLUME = 0x00200000U,
			DDSCAPS2_CUBEMAP = 0x00000200U,
		};

		auto MakeFourCC = [](Uint32 a, Uint32 b, Uint32 c, Uint32 d) { return a | (b << 8u) | (c << 16u) | (d << 24u); };

		constexpr const Char magic[] = "DDS ";
		if (memcmp(magic, bytes, 4) != 0) return false;
		bytes += 4;

		const FileHeader* dds_header = (FileHeader*)bytes;
		bytes += sizeof(FileHeader);

		if (dds_header->dwSize == sizeof(FileHeader) &&
			dds_header->ddpf.dwSize == sizeof(PixelFormatHeader))
		{
			is_srgb = false;
			Uint32 bpp = dds_header->ddpf.dwRGBBitCount;

			Uint32 four_cc = dds_header->ddpf.dwFourCC;
			Bool has_dxgi = four_cc == MakeFourCC('D', 'X', '1', '0');
			const DX10FileHeader* pDx10Header = nullptr;

			if (has_dxgi)
			{
				pDx10Header = (DX10FileHeader*)bytes;
				bytes += sizeof(DX10FileHeader);

				auto ConvertDX10Format = [](DXGI_FORMAT format, GfxFormat& outFormat, Bool& outSRGB)
				{
					if (format == DXGI_FORMAT_BC1_UNORM) { outFormat = GfxFormat::BC1_UNORM;			 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_BC1_UNORM_SRGB) { outFormat = GfxFormat::BC1_UNORM;		 outSRGB = true;	return; }
					if (format == DXGI_FORMAT_BC2_UNORM) { outFormat = GfxFormat::BC2_UNORM;			 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_BC2_UNORM_SRGB) { outFormat = GfxFormat::BC2_UNORM;		 outSRGB = true;	return; }
					if (format == DXGI_FORMAT_BC3_UNORM) { outFormat = GfxFormat::BC3_UNORM;			 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_BC4_UNORM) { outFormat = GfxFormat::BC4_UNORM;			 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_BC5_UNORM) { outFormat = GfxFormat::BC5_UNORM;			 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_BC6H_UF16) { outFormat = GfxFormat::BC6H_UF16;			 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_BC7_UNORM) { outFormat = GfxFormat::BC7_UNORM;			 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_BC7_UNORM_SRGB) { outFormat = GfxFormat::BC7_UNORM;		 outSRGB = true;	return; }
					if (format == DXGI_FORMAT_R32G32B32A32_FLOAT) { outFormat = GfxFormat::R32G32B32A32_FLOAT; outSRGB = false;	return; }
					if (format == DXGI_FORMAT_R32G32_FLOAT) { outFormat = GfxFormat::R32G32_FLOAT;		 outSRGB = false;	return; }
					if (format == DXGI_FORMAT_R9G9B9E5_SHAREDEXP) { outFormat = GfxFormat::R9G9B9E5_SHAREDEXP;	outSRGB = false;	return; }
				};
				ConvertDX10Format((DXGI_FORMAT)pDx10Header->dxgiFormat, format, is_srgb);
			}
			else
			{
				switch (four_cc)
				{
				case MakeFourCC('B', 'C', '4', 'U'):	format = GfxFormat::BC4_UNORM;		break;
				case MakeFourCC('D', 'X', 'T', '1'):	format = GfxFormat::BC1_UNORM;		break;
				case MakeFourCC('D', 'X', 'T', '3'):	format = GfxFormat::BC2_UNORM;		break;
				case MakeFourCC('D', 'X', 'T', '5'):	format = GfxFormat::BC3_UNORM;		break;
				case MakeFourCC('B', 'C', '5', 'U'):	format = GfxFormat::BC5_UNORM;		break;
				case MakeFourCC('A', 'T', 'I', '2'):	format = GfxFormat::BC5_UNORM;		break;
				case 0:
					if (bpp == 32)
					{
						auto TestMask = [=](Uint32 r, Uint32 g, Uint32 b, Uint32 a)
						{
							return dds_header->ddpf.dwRBitMask == r &&
								dds_header->ddpf.dwGBitMask == g &&
								dds_header->ddpf.dwBBitMask == b &&
								dds_header->ddpf.dwABitMask == a;
						};

						if (TestMask(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000)) format = GfxFormat::R8G8B8A8_UNORM;
						else if (TestMask(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000)) format = GfxFormat::B8G8R8A8_UNORM;
						else return false;
					}
					break;
				default:
					return false;
				}
			}

			Bool _is_cubemap = (dds_header->dwCaps2 & 0x0000FC00U) != 0 || (has_dxgi && (pDx10Header->miscFlag & 0x4) != 0);
			Uint32 image_chain_count = 1;
			if (_is_cubemap)
			{
				image_chain_count = 6;
				is_cubemap = true;
			}
			else if (has_dxgi && pDx10Header->arraySize > 1)
			{
				image_chain_count = pDx10Header->arraySize;
			}

			Image* current_image = this;
			for (Uint32 image_idx = 0; image_idx < image_chain_count; ++image_idx)
			{
				Uint64 offset = current_image->SetData(dds_header->dwWidth, dds_header->dwHeight, dds_header->dwDepth, dds_header->dwMipMapCount, bytes);
				bytes += offset;
				if (image_idx < image_chain_count - 1)
				{
					current_image->next_image = std::make_unique<Image>(format);
					current_image = current_image->next_image.get();
				}
			}
		}
		else return false;
		return true;
	}

	Bool Image::LoadSTB(std::string_view texture_path)
	{
		Sint32 components = 0;
		is_hdr = stbi_is_hdr(texture_path.data());
		if (is_hdr)
		{
			Sint32 _width, _height;
			Float* _pixels = stbi_loadf(texture_path.data(), &_width, &_height, &components, 4);
			if (_pixels == nullptr) return false;
			width = (Uint32)_width;
			height = (Uint32)_height;
			depth = 1;
			mip_levels = 1;
			format = GfxFormat::R32G32B32A32_FLOAT;
			pixels.resize(width * height * 4 * sizeof(Float));
			memcpy(pixels.data(), _pixels, pixels.size());
			stbi_image_free(_pixels);
			return true;
		}
		else
		{
			Sint _width, _height;
			stbi_uc* _pixels = stbi_load(texture_path.data(), &_width, &_height, &components, 4);
			if (_pixels == nullptr) return false;
			width = (Uint32)_width;
			height = (Uint32)_height;
			depth = 1;
			mip_levels = 1;
			format = GfxFormat::R8G8B8A8_UNORM;
			pixels.resize(width * height * 4);
			memcpy(pixels.data(), _pixels, pixels.size());
			stbi_image_free(_pixels);
			return true;
		}
	}
}