#pragma once
#include <memory>
#include <optional>
#include "GraphicsDeviceDX12.h"
#include "d3dx12.h"
#include "../Utilities/EnumUtil.h"
#include "../Utilities/StringUtil.h"

namespace adria
{
	enum class EFormat
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
		BC7_UNORM_SRGB
	};
	inline constexpr DXGI_FORMAT ConvertFormat(EFormat _format)
	{
		switch (_format)
		{
		case EFormat::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;

		case EFormat::R32G32B32A32_FLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;

		case EFormat::R32G32B32A32_UINT:
			return DXGI_FORMAT_R32G32B32A32_UINT;

		case EFormat::R32G32B32A32_SINT:
			return DXGI_FORMAT_R32G32B32A32_SINT;

		case EFormat::R32G32B32_FLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;

		case EFormat::R32G32B32_UINT:
			return DXGI_FORMAT_R32G32B32_UINT;

		case EFormat::R32G32B32_SINT:
			return DXGI_FORMAT_R32G32B32_SINT;

		case EFormat::R16G16B16A16_FLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;

		case EFormat::R16G16B16A16_UNORM:
			return DXGI_FORMAT_R16G16B16A16_UNORM;

		case EFormat::R16G16B16A16_UINT:
			return DXGI_FORMAT_R16G16B16A16_UINT;

		case EFormat::R16G16B16A16_SNORM:
			return DXGI_FORMAT_R16G16B16A16_SNORM;

		case EFormat::R16G16B16A16_SINT:
			return DXGI_FORMAT_R16G16B16A16_SINT;

		case EFormat::R32G32_FLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;

		case EFormat::R32G32_UINT:
			return DXGI_FORMAT_R32G32_UINT;

		case EFormat::R32G32_SINT:
			return DXGI_FORMAT_R32G32_SINT;

		case EFormat::R32G8X24_TYPELESS:
			return DXGI_FORMAT_R32G8X24_TYPELESS;

		case EFormat::D32_FLOAT_S8X24_UINT:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

		case EFormat::R10G10B10A2_UNORM:
			return DXGI_FORMAT_R10G10B10A2_UNORM;

		case EFormat::R10G10B10A2_UINT:
			return DXGI_FORMAT_R10G10B10A2_UINT;

		case EFormat::R11G11B10_FLOAT:
			return DXGI_FORMAT_R11G11B10_FLOAT;

		case EFormat::R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;

		case EFormat::R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		case EFormat::R8G8B8A8_UINT:
			return DXGI_FORMAT_R8G8B8A8_UINT;

		case EFormat::R8G8B8A8_SNORM:
			return DXGI_FORMAT_R8G8B8A8_SNORM;

		case EFormat::R8G8B8A8_SINT:
			return DXGI_FORMAT_R8G8B8A8_SINT;

		case EFormat::R16G16_FLOAT:
			return DXGI_FORMAT_R16G16_FLOAT;

		case EFormat::R16G16_UNORM:
			return DXGI_FORMAT_R16G16_UNORM;

		case EFormat::R16G16_UINT:
			return DXGI_FORMAT_R16G16_UINT;

		case EFormat::R16G16_SNORM:
			return DXGI_FORMAT_R16G16_SNORM;

		case EFormat::R16G16_SINT:
			return DXGI_FORMAT_R16G16_SINT;

		case EFormat::R32_TYPELESS:
			return DXGI_FORMAT_R32_TYPELESS;

		case EFormat::D32_FLOAT:
			return DXGI_FORMAT_D32_FLOAT;

		case EFormat::R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;

		case EFormat::R32_UINT:
			return DXGI_FORMAT_R32_UINT;

		case EFormat::R32_SINT:
			return DXGI_FORMAT_R32_SINT;

		case EFormat::R8G8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;

		case EFormat::R8G8_UINT:
			return DXGI_FORMAT_R8G8_UINT;

		case EFormat::R8G8_SNORM:
			return DXGI_FORMAT_R8G8_SNORM;

		case EFormat::R8G8_SINT:
			return DXGI_FORMAT_R8G8_SINT;

		case EFormat::R16_TYPELESS:
			return DXGI_FORMAT_R16_TYPELESS;

		case EFormat::R16_FLOAT:
			return DXGI_FORMAT_R16_FLOAT;

		case EFormat::D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;

		case EFormat::R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;

		case EFormat::R16_UINT:
			return DXGI_FORMAT_R16_UINT;

		case EFormat::R16_SNORM:
			return DXGI_FORMAT_R16_SNORM;

		case EFormat::R16_SINT:
			return DXGI_FORMAT_R16_SINT;

		case EFormat::R8_UNORM:
			return DXGI_FORMAT_R8_UNORM;

		case EFormat::R8_UINT:
			return DXGI_FORMAT_R8_UINT;

		case EFormat::R8_SNORM:
			return DXGI_FORMAT_R8_SNORM;

		case EFormat::R8_SINT:
			return DXGI_FORMAT_R8_SINT;

		case EFormat::BC1_UNORM:
			return DXGI_FORMAT_BC1_UNORM;

		case EFormat::BC1_UNORM_SRGB:
			return DXGI_FORMAT_BC1_UNORM_SRGB;

		case EFormat::BC2_UNORM:
			return DXGI_FORMAT_BC2_UNORM;

		case EFormat::BC2_UNORM_SRGB:
			return DXGI_FORMAT_BC2_UNORM_SRGB;

		case EFormat::BC3_UNORM:
			return DXGI_FORMAT_BC3_UNORM;

		case EFormat::BC3_UNORM_SRGB:
			return DXGI_FORMAT_BC3_UNORM_SRGB;

		case EFormat::BC4_UNORM:
			return DXGI_FORMAT_BC4_UNORM;

		case EFormat::BC4_SNORM:
			return DXGI_FORMAT_BC4_SNORM;

		case EFormat::BC5_UNORM:
			return DXGI_FORMAT_BC5_UNORM;

		case EFormat::BC5_SNORM:
			return DXGI_FORMAT_BC5_SNORM;

		case EFormat::B8G8R8A8_UNORM:
			return DXGI_FORMAT_B8G8R8A8_UNORM;

		case EFormat::B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

		case EFormat::BC6H_UF16:
			return DXGI_FORMAT_BC6H_UF16;

		case EFormat::BC6H_SF16:
			return DXGI_FORMAT_BC6H_SF16;

		case EFormat::BC7_UNORM:
			return DXGI_FORMAT_BC7_UNORM;

		case EFormat::BC7_UNORM_SRGB:
			return DXGI_FORMAT_BC7_UNORM_SRGB;

		}
		return DXGI_FORMAT_UNKNOWN;
	}
	inline constexpr EFormat ConvertDXGIFormat(DXGI_FORMAT _format)
	{
		switch (_format)
		{
		case DXGI_FORMAT_UNKNOWN:
			return EFormat::UNKNOWN;

		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return EFormat::R32G32B32A32_FLOAT;

		case DXGI_FORMAT_R32G32B32A32_UINT:
			return EFormat::R32G32B32A32_UINT;

		case DXGI_FORMAT_R32G32B32A32_SINT:
			return EFormat::R32G32B32A32_SINT;

		case DXGI_FORMAT_R32G32B32_FLOAT:
			return EFormat::R32G32B32_FLOAT;

		case DXGI_FORMAT_R32G32B32_UINT:
			return EFormat::R32G32B32_UINT;

		case DXGI_FORMAT_R32G32B32_SINT:
			return EFormat::R32G32B32_SINT;

		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return EFormat::R16G16B16A16_FLOAT;

		case DXGI_FORMAT_R16G16B16A16_UNORM:
			return EFormat::R16G16B16A16_UNORM;

		case DXGI_FORMAT_R16G16B16A16_UINT:
			return EFormat::R16G16B16A16_UINT;

		case DXGI_FORMAT_R16G16B16A16_SNORM:
			return EFormat::R16G16B16A16_SNORM;

		case DXGI_FORMAT_R16G16B16A16_SINT:
			return EFormat::R16G16B16A16_SINT;

		case DXGI_FORMAT_R32G32_FLOAT:
			return EFormat::R32G32_FLOAT;

		case DXGI_FORMAT_R32G32_UINT:
			return EFormat::R32G32_UINT;

		case DXGI_FORMAT_R32G32_SINT:
			return EFormat::R32G32_SINT;

		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return EFormat::R32G8X24_TYPELESS;

		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return EFormat::D32_FLOAT_S8X24_UINT;

		case DXGI_FORMAT_R10G10B10A2_UNORM:
			return EFormat::R10G10B10A2_UNORM;

		case DXGI_FORMAT_R10G10B10A2_UINT:
			return EFormat::R10G10B10A2_UINT;

		case DXGI_FORMAT_R11G11B10_FLOAT:
			return EFormat::R11G11B10_FLOAT;

		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return EFormat::R8G8B8A8_UNORM;

		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return EFormat::R8G8B8A8_UNORM_SRGB;

		case DXGI_FORMAT_R8G8B8A8_UINT:
			return EFormat::R8G8B8A8_UINT;

		case DXGI_FORMAT_R8G8B8A8_SNORM:
			return EFormat::R8G8B8A8_SNORM;

		case DXGI_FORMAT_R8G8B8A8_SINT:
			return EFormat::R8G8B8A8_SINT;

		case DXGI_FORMAT_R16G16_FLOAT:
			return EFormat::R16G16_FLOAT;

		case DXGI_FORMAT_R16G16_UNORM:
			return EFormat::R16G16_UNORM;

		case DXGI_FORMAT_R16G16_UINT:
			return EFormat::R16G16_UINT;

		case DXGI_FORMAT_R16G16_SNORM:
			return EFormat::R16G16_SNORM;

		case DXGI_FORMAT_R16G16_SINT:
			return EFormat::R16G16_SINT;

		case DXGI_FORMAT_R32_TYPELESS:
			return EFormat::R32_TYPELESS;

		case DXGI_FORMAT_D32_FLOAT:
			return EFormat::D32_FLOAT;

		case DXGI_FORMAT_R32_FLOAT:
			return EFormat::R32_FLOAT;

		case DXGI_FORMAT_R32_UINT:
			return EFormat::R32_UINT;

		case DXGI_FORMAT_R32_SINT:
			return EFormat::R32_SINT;

		case DXGI_FORMAT_R8G8_UNORM:
			return EFormat::R8G8_UNORM;

		case DXGI_FORMAT_R8G8_UINT:
			return EFormat::R8G8_UINT;

		case DXGI_FORMAT_R8G8_SNORM:
			return EFormat::R8G8_SNORM;

		case DXGI_FORMAT_R8G8_SINT:
			return EFormat::R8G8_SINT;

		case DXGI_FORMAT_R16_TYPELESS:
			return EFormat::R16_TYPELESS;

		case DXGI_FORMAT_R16_FLOAT:
			return EFormat::R16_FLOAT;

		case DXGI_FORMAT_D16_UNORM:
			return EFormat::D16_UNORM;

		case DXGI_FORMAT_R16_UNORM:
			return EFormat::R16_UNORM;

		case DXGI_FORMAT_R16_UINT:
			return EFormat::R16_UINT;

		case DXGI_FORMAT_R16_SNORM:
			return EFormat::R16_SNORM;

		case DXGI_FORMAT_R16_SINT:
			return EFormat::R16_SINT;

		case DXGI_FORMAT_R8_UNORM:
			return EFormat::R8_UNORM;

		case DXGI_FORMAT_R8_UINT:
			return EFormat::R8_UINT;

		case DXGI_FORMAT_R8_SNORM:
			return EFormat::R8_SNORM;

		case DXGI_FORMAT_R8_SINT:
			return EFormat::R8_SINT;

		case DXGI_FORMAT_BC1_UNORM:
			return EFormat::BC1_UNORM;

		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return EFormat::BC1_UNORM_SRGB;

		case DXGI_FORMAT_BC2_UNORM:
			return EFormat::BC2_UNORM;

		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return EFormat::BC2_UNORM_SRGB;

		case DXGI_FORMAT_BC3_UNORM:
			return EFormat::BC3_UNORM;

		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return EFormat::BC3_UNORM_SRGB;

		case DXGI_FORMAT_BC4_UNORM:
			return EFormat::BC4_UNORM;

		case DXGI_FORMAT_BC4_SNORM:
			return EFormat::BC4_SNORM;

		case DXGI_FORMAT_BC5_UNORM:
			return EFormat::BC5_UNORM;

		case DXGI_FORMAT_BC5_SNORM:
			return EFormat::BC5_SNORM;

		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return EFormat::B8G8R8A8_UNORM;

		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return EFormat::B8G8R8A8_UNORM_SRGB;

		case DXGI_FORMAT_BC6H_UF16:
			return EFormat::BC6H_UF16;

		case DXGI_FORMAT_BC6H_SF16:
			return EFormat::BC6H_SF16;

		case DXGI_FORMAT_BC7_UNORM:
			return EFormat::BC7_UNORM;

		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return EFormat::BC7_UNORM_SRGB;

		}
		return EFormat::UNKNOWN;
	}
	inline constexpr uint32_t GetFormatStride(EFormat _format)
	{
		DXGI_FORMAT format = ConvertFormat(_format);
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
			ADRIA_ASSERT(false);
			break;
		}

		return 16u;
	}

	enum ESubresourceType : uint8
	{
		SubresourceType_SRV,
		SubresourceType_UAV,
		SubresourceType_RTV,
		SubresourceType_DSV,
		SubresourceType_Invalid
	};

	enum class EBindFlag : uint32
	{
		None = 0,
		ShaderResource = 1 << 0,
		RenderTarget = 1 << 1,
		DepthStencil = 1 << 2,
		UnorderedAccess = 1 << 3,
	};
	DEFINE_ENUM_BIT_OPERATORS(EBindFlag);

	enum class EResourceUsage : uint8
	{
		Default,
		Upload,	   
		Readback
	};

	enum class ETextureMiscFlag : uint32
	{
		None = 0,
		TextureCube = 1 << 0
	};
	DEFINE_ENUM_BIT_OPERATORS(ETextureMiscFlag);

	enum class EBufferMiscFlag : uint32
	{
		None,
		IndirectArgs = 1 << 0,
		BufferRaw = 1 << 1,
		BufferStructured = 1 << 2,
		ConstantBuffer = 1 << 3,
		VertexBuffer = 1 << 4,
		IndexBuffer = 1 << 5,
		AccelStruct = 1 << 6
	};
	DEFINE_ENUM_BIT_OPERATORS(EBufferMiscFlag);

	enum class EResourceState : uint64
	{
		Common = 0,
		VertexAndConstantBuffer = 0x1,
		IndexBuffer = 0x2,
		RenderTarget = 0x4,
		UnorderedAccess = 0x8,
		DepthWrite = 0x10,
		DepthRead = 0x20,
		NonPixelShaderResource = 0x40,
		PixelShaderResource = 0x80,
		AllShaderResource = PixelShaderResource | NonPixelShaderResource,
		IndirectArgument = 0x100,
		CopyDest = 0x200,
		CopySource = 0x400,
		RaytracingAccelerationStructure = 0x800,
		GenericRead = VertexAndConstantBuffer | IndexBuffer | DepthRead | NonPixelShaderResource | PixelShaderResource | IndirectArgument | CopySource,
	};
	DEFINE_ENUM_BIT_OPERATORS(EResourceState);

	inline constexpr bool IsReadState(EResourceState state)
	{
		return HasAnyFlag(state, EResourceState::GenericRead);
	}
	inline constexpr bool IsWriteState(EResourceState state)
	{
		return HasAnyFlag(state, EResourceState::RenderTarget | EResourceState::UnorderedAccess | EResourceState::DepthWrite | EResourceState::CopyDest);
	}
	inline constexpr bool IsValidState(EResourceState state)
	{
		return true;
	}
	inline constexpr D3D12_RESOURCE_STATES ConvertToD3D12ResourceState(EResourceState state)
	{
		D3D12_RESOURCE_STATES api_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasAnyFlag(state, EResourceState::VertexAndConstantBuffer)) api_state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (HasAnyFlag(state, EResourceState::IndexBuffer)) api_state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (HasAnyFlag(state, EResourceState::RenderTarget)) api_state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (HasAnyFlag(state, EResourceState::UnorderedAccess)) api_state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (HasAnyFlag(state, EResourceState::DepthWrite)) api_state |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if (HasAnyFlag(state, EResourceState::DepthRead)) api_state |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (HasAnyFlag(state, EResourceState::NonPixelShaderResource)) api_state |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (HasAnyFlag(state, EResourceState::PixelShaderResource)) api_state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (HasAnyFlag(state, EResourceState::IndirectArgument)) api_state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (HasAnyFlag(state, EResourceState::CopyDest)) api_state |= D3D12_RESOURCE_STATE_COPY_DEST;
		if (HasAnyFlag(state, EResourceState::CopySource)) api_state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if (HasAnyFlag(state, EResourceState::RaytracingAccelerationStructure)) api_state |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		return api_state;
	}
}