#pragma once
#include <memory>
#include <optional>
#include "GraphicsDeviceDX12.h"
#include "d3dx12.h"
#include "../Utilities/EnumUtil.h"
#include "../Utilities/StringUtil.h"

namespace adria
{
	inline constexpr uint32_t GetFormatStride(DXGI_FORMAT format)
	{
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
		Default,	// CPU no access, GPU read/write
		Upload,	    // CPU write, GPU read
		Readback,	// CPU read, GPU write
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
		return true; //todo
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