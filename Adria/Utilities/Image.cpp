#include "Image.h"
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
			std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](char c) {return std::tolower(c); });

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
		bool result;
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
			ADRIA_ASSERT(false && "Unsupported Texture Format!");
		}
		ADRIA_ASSERT(result);
	}

	void Image::SetData(uint32 _width, uint32 _height, uint32 _depth, uint32 _mip_levels, void const* _data)
	{
		width = std::max(_width, 1u);
		height = std::max(_height, 1u);
		depth = std::max(_depth, 1u);
		mip_levels = _mip_levels;
		pixels.resize(GetTextureByteSize(format, width, height, depth, mip_levels));
		memcpy(pixels.data(), _data, pixels.size());
	}

	bool Image::LoadDDS(std::string_view texture_path)
	{
		//https://github.com/simco50/D3D12_Research/blob/master/D3D12/Content/Image.cpp - LoadDDS

		FILE* pFile = nullptr;
		fopen_s(&pFile, texture_path.data(), "rb");
		if (!pFile)
			return false;

		fseek(pFile, 0, SEEK_END);
		std::vector<char> data((size_t)ftell(pFile));
		fseek(pFile, 0, SEEK_SET);
		fread(data.data(), data.size(), 1, pFile);

	char* pBytes = data.data();
#pragma pack(push,1)
		struct PixelFormatHeader
		{
			uint32 dwSize;
			uint32 dwFlags;
			uint32 dwFourCC;
			uint32 dwRGBBitCount;
			uint32 dwRBitMask;
			uint32 dwGBitMask;
			uint32 dwBBitMask;
			uint32 dwABitMask;
		};
#pragma pack(pop)

		// .DDS header.
#pragma pack(push,1)
		struct FileHeader
		{
			uint32 dwSize;
			uint32 dwFlags;
			uint32 dwHeight;
			uint32 dwWidth;
			uint32 dwLinearSize;
			uint32 dwDepth;
			uint32 dwMipMapCount;
			uint32 dwReserved1[11];
			PixelFormatHeader ddpf;
			uint32 dwCaps;
			uint32 dwCaps2;
			uint32 dwCaps3;
			uint32 dwCaps4;
			uint32 dwReserved2;
		};
#pragma pack(pop)

		// .DDS 10 header.
#pragma pack(push,1)
		struct DX10FileHeader
		{
			uint32 dxgiFormat;
			uint32 resourceDimension;
			uint32 miscFlag;
			uint32 arraySize;
			uint32 reserved;
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

		auto MakeFourCC = [](uint32 a, uint32 b, uint32 c, uint32 d) { return a | (b << 8u) | (c << 16u) | (d << 24u); };

		constexpr const char pMagic[] = "DDS ";
		if (memcmp(pMagic, pBytes, 4) != 0) return false;
		pBytes += 4;

		const FileHeader* pHeader = (FileHeader*)pBytes;
		pBytes += sizeof(FileHeader);

		if (pHeader->dwSize == sizeof(FileHeader) &&
			pHeader->ddpf.dwSize == sizeof(PixelFormatHeader))
		{
			is_srgb = false;
			uint32 bpp = pHeader->ddpf.dwRGBBitCount;

			uint32 fourCC = pHeader->ddpf.dwFourCC;
			bool hasDxgi = fourCC == MakeFourCC('D', 'X', '1', '0');
			const DX10FileHeader* pDx10Header = nullptr;

			if (hasDxgi)
			{
				pDx10Header = (DX10FileHeader*)pBytes;
				pBytes += sizeof(DX10FileHeader);

				auto ConvertDX10Format = [](DXGI_FORMAT format, GfxFormat& outFormat, bool& outSRGB)
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
				};
				ConvertDX10Format((DXGI_FORMAT)pDx10Header->dxgiFormat, format, is_srgb);
			}
			else
			{
				switch (fourCC)
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
						auto TestMask = [=](uint32 r, uint32 g, uint32 b, uint32 a)
						{
							return pHeader->ddpf.dwRBitMask == r &&
								pHeader->ddpf.dwGBitMask == g &&
								pHeader->ddpf.dwBBitMask == b &&
								pHeader->ddpf.dwABitMask == a;
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

			bool isCubemap = (pHeader->dwCaps2 & 0x0000FC00U) != 0 || (hasDxgi && (pDx10Header->miscFlag & 0x4) != 0);
			uint32 imageChainCount = 1;
			if (isCubemap)
			{
				imageChainCount = 6;
				is_cubemap = true;
			}
			else if (hasDxgi && pDx10Header->arraySize > 1)
			{
				imageChainCount = pDx10Header->arraySize;
			}

			Image* pCurrentImage = this;
			for (uint32 imageIdx = 0; imageIdx < imageChainCount; ++imageIdx)
			{
				pCurrentImage->SetData(pHeader->dwWidth, pHeader->dwHeight, pHeader->dwDepth, pHeader->dwMipMapCount, pBytes);

				if (imageIdx < imageChainCount - 1)
				{
					pCurrentImage->next_image = std::make_unique<Image>(format);
					pCurrentImage = pCurrentImage->next_image.get();
				}
			}
		}
		else return false;
		return true;
	}

	bool Image::LoadSTB(std::string_view  texture_path)
	{
		int32 components = 0;
		is_hdr = stbi_is_hdr(texture_path.data());
		if (is_hdr)
		{
			int32 _width, _height;
			float* _pixels = stbi_loadf(texture_path.data(), &_width, &_height, &components, 4);
			if (_pixels == nullptr) return false;
			width = (uint32)_width;
			height = (uint32)_height;
			depth = 1;
			mip_levels = 1;
			format = GfxFormat::R32G32B32A32_FLOAT;
			pixels.resize(width * height * 4 * sizeof(float));
			memcpy(pixels.data(), _pixels, pixels.size());
			stbi_image_free(_pixels);
			return true;
		}
		else
		{
			int _width, _height;
			stbi_uc* _pixels = stbi_load(texture_path.data(), &_width, &_height, &components, 4);
			if (_pixels == nullptr) return false;
			width = (uint32)_width;
			height = (uint32)_height;
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