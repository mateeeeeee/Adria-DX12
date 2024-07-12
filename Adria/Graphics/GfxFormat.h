#pragma once

namespace adria
{
	constexpr inline uint32 DivideAndRoundUp(uint32 nominator, uint32 denominator)
	{
		return (nominator + denominator - 1) / denominator;
	}

	enum class GfxFormat
	{
		UNKNOWN,
		R32G32B32A32_FLOAT,
		R32G32B32A32_UINT,
		R32G32B32A32_SINT,
		R32G32B32_FLOAT,
		R32G32B32_UINT,
		R32G32B32_SINT,
		R16G16B16A16_FLOAT,
		R16G16B16A16_UNORM,
		R16G16B16A16_UINT,
		R16G16B16A16_SNORM,
		R16G16B16A16_SINT,
		R32G32_FLOAT,
		R32G32_UINT,
		R32G32_SINT,
		R32G8X24_TYPELESS,
		D32_FLOAT_S8X24_UINT,
		R10G10B10A2_UNORM,
		R10G10B10A2_UINT,
		R11G11B10_FLOAT,
		R8G8B8A8_UNORM,
		R8G8B8A8_UNORM_SRGB,
		R8G8B8A8_UINT,
		R8G8B8A8_SNORM,
		R8G8B8A8_SINT,
		B8G8R8A8_UNORM,
		B8G8R8A8_UNORM_SRGB,
		R16G16_FLOAT,
		R16G16_UNORM,
		R16G16_UINT,
		R16G16_SNORM,
		R16G16_SINT,
		R32_TYPELESS,
		D32_FLOAT,
		R32_FLOAT,
		R32_UINT,
		R32_SINT,
		R24G8_TYPELESS,
		D24_UNORM_S8_UINT,
		R8G8_UNORM,
		R8G8_UINT,
		R8G8_SNORM,
		R8G8_SINT,
		R16_TYPELESS,
		R16_FLOAT,
		D16_UNORM,
		R16_UNORM,
		R16_UINT,
		R16_SNORM,
		R16_SINT,
		R8_UNORM,
		R8_UINT,
		R8_SNORM,
		R8_SINT,
		BC1_UNORM,
		BC1_UNORM_SRGB,
		BC2_UNORM,
		BC2_UNORM_SRGB,
		BC3_UNORM,
		BC3_UNORM_SRGB,
		BC4_UNORM,
		BC4_SNORM,
		BC5_UNORM,
		BC5_SNORM,
		BC6H_UF16,
		BC6H_SF16,
		BC7_UNORM,
		BC7_UNORM_SRGB,
		R9G9B9E5_SHAREDEXP
	};
	inline constexpr DXGI_FORMAT ConvertGfxFormat(GfxFormat _format)
	{
		switch (_format)
		{
		case GfxFormat::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;

		case GfxFormat::R32G32B32A32_FLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;

		case GfxFormat::R32G32B32A32_UINT:
			return DXGI_FORMAT_R32G32B32A32_UINT;

		case GfxFormat::R32G32B32A32_SINT:
			return DXGI_FORMAT_R32G32B32A32_SINT;

		case GfxFormat::R32G32B32_FLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;

		case GfxFormat::R32G32B32_UINT:
			return DXGI_FORMAT_R32G32B32_UINT;

		case GfxFormat::R32G32B32_SINT:
			return DXGI_FORMAT_R32G32B32_SINT;

		case GfxFormat::R16G16B16A16_FLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;

		case GfxFormat::R16G16B16A16_UNORM:
			return DXGI_FORMAT_R16G16B16A16_UNORM;

		case GfxFormat::R16G16B16A16_UINT:
			return DXGI_FORMAT_R16G16B16A16_UINT;

		case GfxFormat::R16G16B16A16_SNORM:
			return DXGI_FORMAT_R16G16B16A16_SNORM;

		case GfxFormat::R16G16B16A16_SINT:
			return DXGI_FORMAT_R16G16B16A16_SINT;

		case GfxFormat::R32G32_FLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;

		case GfxFormat::R32G32_UINT:
			return DXGI_FORMAT_R32G32_UINT;

		case GfxFormat::R32G32_SINT:
			return DXGI_FORMAT_R32G32_SINT;

		case GfxFormat::R32G8X24_TYPELESS:
			return DXGI_FORMAT_R32G8X24_TYPELESS;

		case GfxFormat::D32_FLOAT_S8X24_UINT:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

		case GfxFormat::R10G10B10A2_UNORM:
			return DXGI_FORMAT_R10G10B10A2_UNORM;

		case GfxFormat::R10G10B10A2_UINT:
			return DXGI_FORMAT_R10G10B10A2_UINT;

		case GfxFormat::R11G11B10_FLOAT:
			return DXGI_FORMAT_R11G11B10_FLOAT;

		case GfxFormat::R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;

		case GfxFormat::R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		case GfxFormat::R8G8B8A8_UINT:
			return DXGI_FORMAT_R8G8B8A8_UINT;

		case GfxFormat::R8G8B8A8_SNORM:
			return DXGI_FORMAT_R8G8B8A8_SNORM;

		case GfxFormat::R8G8B8A8_SINT:
			return DXGI_FORMAT_R8G8B8A8_SINT;

		case GfxFormat::R16G16_FLOAT:
			return DXGI_FORMAT_R16G16_FLOAT;

		case GfxFormat::R16G16_UNORM:
			return DXGI_FORMAT_R16G16_UNORM;

		case GfxFormat::R16G16_UINT:
			return DXGI_FORMAT_R16G16_UINT;

		case GfxFormat::R16G16_SNORM:
			return DXGI_FORMAT_R16G16_SNORM;

		case GfxFormat::R16G16_SINT:
			return DXGI_FORMAT_R16G16_SINT;

		case GfxFormat::R32_TYPELESS:
			return DXGI_FORMAT_R32_TYPELESS;

		case GfxFormat::D32_FLOAT:
			return DXGI_FORMAT_D32_FLOAT;

		case GfxFormat::R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;

		case GfxFormat::R32_UINT:
			return DXGI_FORMAT_R32_UINT;

		case GfxFormat::R32_SINT:
			return DXGI_FORMAT_R32_SINT;

		case GfxFormat::R8G8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;

		case GfxFormat::R8G8_UINT:
			return DXGI_FORMAT_R8G8_UINT;

		case GfxFormat::R8G8_SNORM:
			return DXGI_FORMAT_R8G8_SNORM;

		case GfxFormat::R8G8_SINT:
			return DXGI_FORMAT_R8G8_SINT;

		case GfxFormat::R16_TYPELESS:
			return DXGI_FORMAT_R16_TYPELESS;

		case GfxFormat::R16_FLOAT:
			return DXGI_FORMAT_R16_FLOAT;

		case GfxFormat::D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;

		case GfxFormat::R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;

		case GfxFormat::R16_UINT:
			return DXGI_FORMAT_R16_UINT;

		case GfxFormat::R16_SNORM:
			return DXGI_FORMAT_R16_SNORM;

		case GfxFormat::R16_SINT:
			return DXGI_FORMAT_R16_SINT;

		case GfxFormat::R8_UNORM:
			return DXGI_FORMAT_R8_UNORM;

		case GfxFormat::R8_UINT:
			return DXGI_FORMAT_R8_UINT;

		case GfxFormat::R8_SNORM:
			return DXGI_FORMAT_R8_SNORM;

		case GfxFormat::R8_SINT:
			return DXGI_FORMAT_R8_SINT;

		case GfxFormat::BC1_UNORM:
			return DXGI_FORMAT_BC1_UNORM;

		case GfxFormat::BC1_UNORM_SRGB:
			return DXGI_FORMAT_BC1_UNORM_SRGB;

		case GfxFormat::BC2_UNORM:
			return DXGI_FORMAT_BC2_UNORM;

		case GfxFormat::BC2_UNORM_SRGB:
			return DXGI_FORMAT_BC2_UNORM_SRGB;

		case GfxFormat::BC3_UNORM:
			return DXGI_FORMAT_BC3_UNORM;

		case GfxFormat::BC3_UNORM_SRGB:
			return DXGI_FORMAT_BC3_UNORM_SRGB;

		case GfxFormat::BC4_UNORM:
			return DXGI_FORMAT_BC4_UNORM;

		case GfxFormat::BC4_SNORM:
			return DXGI_FORMAT_BC4_SNORM;

		case GfxFormat::BC5_UNORM:
			return DXGI_FORMAT_BC5_UNORM;

		case GfxFormat::BC5_SNORM:
			return DXGI_FORMAT_BC5_SNORM;

		case GfxFormat::B8G8R8A8_UNORM:
			return DXGI_FORMAT_B8G8R8A8_UNORM;

		case GfxFormat::B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

		case GfxFormat::BC6H_UF16:
			return DXGI_FORMAT_BC6H_UF16;

		case GfxFormat::BC6H_SF16:
			return DXGI_FORMAT_BC6H_SF16;

		case GfxFormat::BC7_UNORM:
			return DXGI_FORMAT_BC7_UNORM;

		case GfxFormat::BC7_UNORM_SRGB:
			return DXGI_FORMAT_BC7_UNORM_SRGB;
		case GfxFormat::R9G9B9E5_SHAREDEXP:
			return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
		}
		return DXGI_FORMAT_UNKNOWN;
	}
	inline constexpr GfxFormat ConvertDXGIFormat(DXGI_FORMAT _format)
	{
		switch (_format)
		{
		case DXGI_FORMAT_UNKNOWN:
			return GfxFormat::UNKNOWN;

		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return GfxFormat::R32G32B32A32_FLOAT;

		case DXGI_FORMAT_R32G32B32A32_UINT:
			return GfxFormat::R32G32B32A32_UINT;

		case DXGI_FORMAT_R32G32B32A32_SINT:
			return GfxFormat::R32G32B32A32_SINT;

		case DXGI_FORMAT_R32G32B32_FLOAT:
			return GfxFormat::R32G32B32_FLOAT;

		case DXGI_FORMAT_R32G32B32_UINT:
			return GfxFormat::R32G32B32_UINT;

		case DXGI_FORMAT_R32G32B32_SINT:
			return GfxFormat::R32G32B32_SINT;

		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return GfxFormat::R16G16B16A16_FLOAT;

		case DXGI_FORMAT_R16G16B16A16_UNORM:
			return GfxFormat::R16G16B16A16_UNORM;

		case DXGI_FORMAT_R16G16B16A16_UINT:
			return GfxFormat::R16G16B16A16_UINT;

		case DXGI_FORMAT_R16G16B16A16_SNORM:
			return GfxFormat::R16G16B16A16_SNORM;

		case DXGI_FORMAT_R16G16B16A16_SINT:
			return GfxFormat::R16G16B16A16_SINT;

		case DXGI_FORMAT_R32G32_FLOAT:
			return GfxFormat::R32G32_FLOAT;

		case DXGI_FORMAT_R32G32_UINT:
			return GfxFormat::R32G32_UINT;

		case DXGI_FORMAT_R32G32_SINT:
			return GfxFormat::R32G32_SINT;

		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return GfxFormat::R32G8X24_TYPELESS;

		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return GfxFormat::D32_FLOAT_S8X24_UINT;

		case DXGI_FORMAT_R10G10B10A2_UNORM:
			return GfxFormat::R10G10B10A2_UNORM;

		case DXGI_FORMAT_R10G10B10A2_UINT:
			return GfxFormat::R10G10B10A2_UINT;

		case DXGI_FORMAT_R11G11B10_FLOAT:
			return GfxFormat::R11G11B10_FLOAT;

		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return GfxFormat::R8G8B8A8_UNORM;

		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return GfxFormat::R8G8B8A8_UNORM_SRGB;

		case DXGI_FORMAT_R8G8B8A8_UINT:
			return GfxFormat::R8G8B8A8_UINT;

		case DXGI_FORMAT_R8G8B8A8_SNORM:
			return GfxFormat::R8G8B8A8_SNORM;

		case DXGI_FORMAT_R8G8B8A8_SINT:
			return GfxFormat::R8G8B8A8_SINT;

		case DXGI_FORMAT_R16G16_FLOAT:
			return GfxFormat::R16G16_FLOAT;

		case DXGI_FORMAT_R16G16_UNORM:
			return GfxFormat::R16G16_UNORM;

		case DXGI_FORMAT_R16G16_UINT:
			return GfxFormat::R16G16_UINT;

		case DXGI_FORMAT_R16G16_SNORM:
			return GfxFormat::R16G16_SNORM;

		case DXGI_FORMAT_R16G16_SINT:
			return GfxFormat::R16G16_SINT;

		case DXGI_FORMAT_R32_TYPELESS:
			return GfxFormat::R32_TYPELESS;

		case DXGI_FORMAT_D32_FLOAT:
			return GfxFormat::D32_FLOAT;

		case DXGI_FORMAT_R32_FLOAT:
			return GfxFormat::R32_FLOAT;

		case DXGI_FORMAT_R32_UINT:
			return GfxFormat::R32_UINT;

		case DXGI_FORMAT_R32_SINT:
			return GfxFormat::R32_SINT;

		case DXGI_FORMAT_R8G8_UNORM:
			return GfxFormat::R8G8_UNORM;

		case DXGI_FORMAT_R8G8_UINT:
			return GfxFormat::R8G8_UINT;

		case DXGI_FORMAT_R8G8_SNORM:
			return GfxFormat::R8G8_SNORM;

		case DXGI_FORMAT_R8G8_SINT:
			return GfxFormat::R8G8_SINT;

		case DXGI_FORMAT_R16_TYPELESS:
			return GfxFormat::R16_TYPELESS;

		case DXGI_FORMAT_R16_FLOAT:
			return GfxFormat::R16_FLOAT;

		case DXGI_FORMAT_D16_UNORM:
			return GfxFormat::D16_UNORM;

		case DXGI_FORMAT_R16_UNORM:
			return GfxFormat::R16_UNORM;

		case DXGI_FORMAT_R16_UINT:
			return GfxFormat::R16_UINT;

		case DXGI_FORMAT_R16_SNORM:
			return GfxFormat::R16_SNORM;

		case DXGI_FORMAT_R16_SINT:
			return GfxFormat::R16_SINT;

		case DXGI_FORMAT_R8_UNORM:
			return GfxFormat::R8_UNORM;

		case DXGI_FORMAT_R8_UINT:
			return GfxFormat::R8_UINT;

		case DXGI_FORMAT_R8_SNORM:
			return GfxFormat::R8_SNORM;

		case DXGI_FORMAT_R8_SINT:
			return GfxFormat::R8_SINT;

		case DXGI_FORMAT_BC1_UNORM:
			return GfxFormat::BC1_UNORM;

		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return GfxFormat::BC1_UNORM_SRGB;

		case DXGI_FORMAT_BC2_UNORM:
			return GfxFormat::BC2_UNORM;

		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return GfxFormat::BC2_UNORM_SRGB;

		case DXGI_FORMAT_BC3_UNORM:
			return GfxFormat::BC3_UNORM;

		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return GfxFormat::BC3_UNORM_SRGB;

		case DXGI_FORMAT_BC4_UNORM:
			return GfxFormat::BC4_UNORM;

		case DXGI_FORMAT_BC4_SNORM:
			return GfxFormat::BC4_SNORM;

		case DXGI_FORMAT_BC5_UNORM:
			return GfxFormat::BC5_UNORM;

		case DXGI_FORMAT_BC5_SNORM:
			return GfxFormat::BC5_SNORM;

		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return GfxFormat::B8G8R8A8_UNORM;

		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return GfxFormat::B8G8R8A8_UNORM_SRGB;

		case DXGI_FORMAT_BC6H_UF16:
			return GfxFormat::BC6H_UF16;

		case DXGI_FORMAT_BC6H_SF16:
			return GfxFormat::BC6H_SF16;

		case DXGI_FORMAT_BC7_UNORM:
			return GfxFormat::BC7_UNORM;

		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return GfxFormat::BC7_UNORM_SRGB;
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			return GfxFormat::R9G9B9E5_SHAREDEXP;
		}
		return GfxFormat::UNKNOWN;
	}
	inline constexpr uint32 GetGfxFormatStride(GfxFormat _format)
	{
		DXGI_FORMAT format = ConvertGfxFormat(_format);
		switch (format)
		{
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC4_UNORM:
			return 8u;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return 16u;
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			return 12u;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
			return 8u;
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return 8u;
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			return 4u;
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
			return 2u;
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
			return 1u;
		default:
			break;
		}
		return 16u;
	}
	inline constexpr uint32 GetGfxFormatBlockSize(GfxFormat _format)
	{
		switch (_format)
		{
		case GfxFormat::BC1_UNORM:
		case GfxFormat::BC2_UNORM:
		case GfxFormat::BC3_UNORM:
		case GfxFormat::BC4_UNORM:
		case GfxFormat::BC5_UNORM:
		case GfxFormat::BC5_SNORM:
		case GfxFormat::BC6H_UF16:
		case GfxFormat::BC6H_SF16:
		case GfxFormat::BC7_UNORM:
			return 4;
		}
		return 1;
	}
	inline constexpr bool IsGfxFormatDepth(GfxFormat _format)
	{
		switch (_format)
		{
		case GfxFormat::D16_UNORM:
		case GfxFormat::D32_FLOAT:
		case GfxFormat::D24_UNORM_S8_UINT:
		case GfxFormat::D32_FLOAT_S8X24_UINT:
			return true;
		}
		return false;
	}

	inline uint64 GetRowPitch(GfxFormat format, uint32 width, uint32 mip_index = 0)
	{
		uint64 num_blocks = std::max(1u, DivideAndRoundUp(width >> mip_index, GetGfxFormatBlockSize(format)));
		return num_blocks * GetGfxFormatStride(format);
	}
	inline uint64 GetSlicePitch(GfxFormat format, uint32 width, uint32 height, uint32 mip_index = 0)
	{
		uint64 num_blocks_x = std::max(1u, DivideAndRoundUp(width >> mip_index, GetGfxFormatBlockSize(format)));
		uint64 num_blocks_y = std::max(1u, DivideAndRoundUp(height >> mip_index, GetGfxFormatBlockSize(format)));
		return num_blocks_x * num_blocks_y * GetGfxFormatStride(format);
	}
	inline uint64 GetTextureMipByteSize(GfxFormat format, uint32 width, uint32 height, uint32 depth, uint32 mip_index)
	{
		return GetSlicePitch(format, width, height, mip_index) * std::max(1u, depth >> mip_index);
	}
	inline uint64 GetTextureByteSize(GfxFormat format, uint32 width, uint32 height, uint32 depth = 1, uint32 mip_count = 1)
	{
		uint64 size = 0;
		for (uint32 mip_level = 0; mip_level < mip_count; ++mip_level)
		{
			size += GetTextureMipByteSize(format, width, height, depth, mip_level);
		}
		return size;
	}
	inline char const* GfxFormatToString(GfxFormat format)
	{
		switch (format)
		{
		case GfxFormat::UNKNOWN: return "UNKNOWN";
		case GfxFormat::R32G32B32A32_FLOAT: return "R32G32B32A32_FLOAT";
		case GfxFormat::R32G32B32A32_UINT: return "R32G32B32A32_UINT";
		case GfxFormat::R32G32B32A32_SINT: return "R32G32B32A32_SINT";
		case GfxFormat::R32G32B32_FLOAT: return "R32G32B32_FLOAT";
		case GfxFormat::R32G32B32_UINT: return "R32G32B32_UINT";
		case GfxFormat::R32G32B32_SINT: return "R32G32B32_SINT";
		case GfxFormat::R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
		case GfxFormat::R16G16B16A16_UNORM: return "R16G16B16A16_UNORM";	
		case GfxFormat::R16G16B16A16_UINT: return "R16G16B16A16_UINT";
		case GfxFormat::R16G16B16A16_SNORM: return "R16G16B16A16_SNORM";
		case GfxFormat::R16G16B16A16_SINT: return "R16G16B16A16_SINT";
		case GfxFormat::R32G32_FLOAT: return "R32G32_FLOAT";
		case GfxFormat::R32G32_UINT: return "R32G32_UINT";
		case GfxFormat::R32G32_SINT: return "R32G32_SINT";
		case GfxFormat::R32G8X24_TYPELESS: return "R32G8X24_TYPELESS";	
		case GfxFormat::D32_FLOAT_S8X24_UINT: return "D32_FLOAT_S8X24_UINT";
		case GfxFormat::R10G10B10A2_UNORM: return "R10G10B10A2_UNORM";	
		case GfxFormat::R10G10B10A2_UINT: return "R10G10B10A2_UINT";
		case GfxFormat::R11G11B10_FLOAT: return "R11G11B10_FLOAT";
		case GfxFormat::R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
		case GfxFormat::R8G8B8A8_UNORM_SRGB: return "R8G8B8A8_UNORM_SRGB";	
		case GfxFormat::R8G8B8A8_UINT: return "R8G8B8A8_UINT";
		case GfxFormat::R8G8B8A8_SNORM: return "R8G8B8A8_SNORM";
		case GfxFormat::R8G8B8A8_SINT: return "R8G8B8A8_SINT";
		case GfxFormat::B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
		case GfxFormat::B8G8R8A8_UNORM_SRGB: return "B8G8R8A8_UNORM_SRGB";
		case GfxFormat::R16G16_FLOAT: return "R16G16_FLOAT";
		case GfxFormat::R16G16_UNORM: return "R16G16_UNORM";
		case GfxFormat::R16G16_UINT: return "R16G16_UINT";
		case GfxFormat::R16G16_SNORM: return "R16G16_SNORM";
		case GfxFormat::R16G16_SINT: return "R16G16_SINT";
		case GfxFormat::R32_TYPELESS: return "R32_TYPELESS";
		case GfxFormat::D32_FLOAT: return "D32_FLOAT";
		case GfxFormat::R32_FLOAT: return "R32_FLOAT";
		case GfxFormat::R32_UINT: return "R32_UINT";
		case GfxFormat::R32_SINT: return "R32_SINT";
		case GfxFormat::R24G8_TYPELESS: return "R24G8_TYPELESS";
		case GfxFormat::D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";	
		case GfxFormat::R8G8_UNORM: return "R8G8_UNORM";	
		case GfxFormat::R8G8_UINT: return "R8G8_UINT";	
		case GfxFormat::R8G8_SNORM: return "R8G8_SNORM";	
		case GfxFormat::R8G8_SINT: return "R8G8_SINT";
		case GfxFormat::R16_TYPELESS: return "R16_TYPELESS";
		case GfxFormat::R16_FLOAT: return "R16_FLOAT";
		case GfxFormat::D16_UNORM: return "D16_UNORM";
		case GfxFormat::R16_UNORM: return "R16_UNORM";
		case GfxFormat::R16_UINT: return "R16_UINT";
		case GfxFormat::R16_SNORM: return "R16_SNORM";
		case GfxFormat::R16_SINT: return "R16_SINT";
		case GfxFormat::R8_UNORM: return "R8_UNORM";
		case GfxFormat::R8_UINT: return "R8_UINT";
		case GfxFormat::R8_SNORM: return "R8_SNORM";
		case GfxFormat::R8_SINT: return "R8_SINT";
		case GfxFormat::BC1_UNORM: return "BC1_UNORM";
		case GfxFormat::BC1_UNORM_SRGB: return "BC1_UNORM_SRGB";
		case GfxFormat::BC2_UNORM: return "BC2_UNORM";
		case GfxFormat::BC2_UNORM_SRGB: return "BC2_UNORM_SRGB";
		case GfxFormat::BC3_UNORM: return "BC3_UNORM";
		case GfxFormat::BC3_UNORM_SRGB: return "BC3_UNORM_SRGB";
		case GfxFormat::BC4_UNORM: return "BC4_UNORM";
		case GfxFormat::BC4_SNORM: return "BC4_SNORM";
		case GfxFormat::BC5_UNORM: return "BC5_UNORM";
		case GfxFormat::BC5_SNORM: return "BC5_SNORM";
		case GfxFormat::BC6H_UF16: return "BC6H_UF16";
		case GfxFormat::BC6H_SF16: return "BC6H_SF16";
		case GfxFormat::BC7_UNORM: return "BC7_UNORM";
		case GfxFormat::BC7_UNORM_SRGB: return "BC7_UNORM_SRGB";
		case GfxFormat::R9G9B9E5_SHAREDEXP: return "R9G9B9E5_SHAREDEXP";
		default: return "INVALID";
		}
	}
}