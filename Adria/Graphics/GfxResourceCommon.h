#pragma once
#include <memory>
#include <optional>
#include "GfxDevice.h"
#include "GfxFormat.h"
#include "d3dx12.h"
#include "../Utilities/EnumUtil.h"
#include "../Utilities/StringUtil.h"

namespace adria
{
	enum class GfxSubresourceType : uint8
	{
		SRV,
		UAV,
		RTV,
		DSV,
		Invalid
	};

	enum class GfxBindFlag : uint32
	{
		None = 0,
		ShaderResource = 1 << 0,
		RenderTarget = 1 << 1,
		DepthStencil = 1 << 2,
		UnorderedAccess = 1 << 3,
	};
	DEFINE_ENUM_BIT_OPERATORS(GfxBindFlag);

	enum class GfxResourceUsage : uint8
	{
		Default,
		Upload,	   
		Readback
	};

	enum class GfxTextureMiscFlag : uint32
	{
		None = 0,
		TextureCube = 1 << 0
	};
	DEFINE_ENUM_BIT_OPERATORS(GfxTextureMiscFlag);

	enum class GfxBufferMiscFlag : uint32
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
	DEFINE_ENUM_BIT_OPERATORS(GfxBufferMiscFlag);

	enum class GfxResourceState : uint64
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
		Present = 0x1000,
		GenericRead = VertexAndConstantBuffer | IndexBuffer | DepthRead | NonPixelShaderResource | PixelShaderResource | IndirectArgument | CopySource,
	};
	DEFINE_ENUM_BIT_OPERATORS(GfxResourceState);

	inline constexpr bool IsReadState(GfxResourceState state)
	{
		return HasAnyFlag(state, GfxResourceState::GenericRead);
	}
	inline constexpr bool IsWriteState(GfxResourceState state)
	{
		return HasAnyFlag(state, GfxResourceState::RenderTarget | GfxResourceState::UnorderedAccess | GfxResourceState::DepthWrite | GfxResourceState::CopyDest);
	}
	inline constexpr bool IsValidState(GfxResourceState state)
	{
		return true;
	}
	inline constexpr D3D12_RESOURCE_STATES ConvertToD3D12ResourceState(GfxResourceState state)
	{
		D3D12_RESOURCE_STATES api_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasAnyFlag(state, GfxResourceState::VertexAndConstantBuffer)) api_state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (HasAnyFlag(state, GfxResourceState::IndexBuffer)) api_state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (HasAnyFlag(state, GfxResourceState::RenderTarget)) api_state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (HasAnyFlag(state, GfxResourceState::UnorderedAccess)) api_state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (HasAnyFlag(state, GfxResourceState::DepthWrite)) api_state |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if (HasAnyFlag(state, GfxResourceState::DepthRead)) api_state |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (HasAnyFlag(state, GfxResourceState::NonPixelShaderResource)) api_state |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (HasAnyFlag(state, GfxResourceState::PixelShaderResource)) api_state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (HasAnyFlag(state, GfxResourceState::IndirectArgument)) api_state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (HasAnyFlag(state, GfxResourceState::CopyDest)) api_state |= D3D12_RESOURCE_STATE_COPY_DEST;
		if (HasAnyFlag(state, GfxResourceState::CopySource)) api_state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if (HasAnyFlag(state, GfxResourceState::Present)) api_state |= D3D12_RESOURCE_STATE_PRESENT;
		if (HasAnyFlag(state, GfxResourceState::RaytracingAccelerationStructure)) api_state |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		return api_state;
	}
}