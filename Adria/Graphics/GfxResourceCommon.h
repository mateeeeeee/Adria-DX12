#pragma once
#include "GfxFormat.h"
#include "D3D12MemAlloc.h"
#include "Utilities/EnumUtil.h"
#include "Utilities/StringUtil.h"
#include "Utilities/AutoRefCountPtr.h"
#include "Utilities/Releasable.h"

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

	struct GfxClearValue
	{
		enum class GfxActiveMember
		{
			None,
			Color,
			DepthStencil
		};
		struct GfxClearColor
		{
			GfxClearColor(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 0.0f)
				: color{ r, g, b, a }
			{
			}
			GfxClearColor(float(&_color)[4])
				: color{ _color[0], _color[1], _color[2], _color[3] }
			{
			}
			GfxClearColor(GfxClearColor const& other)
				: color{ other.color[0], other.color[1], other.color[2], other.color[3] }
			{
			}

			bool operator==(GfxClearColor const& other) const
			{
				return memcmp(color, other.color, sizeof(color)) == 0;
			}

			float color[4];
		};
		struct GfxClearDepthStencil
		{
			GfxClearDepthStencil(float depth = 0.0f, uint8 stencil = 1)
				: depth(depth), stencil(stencil)
			{}
			float depth;
			uint8 stencil;
		};

		GfxClearValue() : active_member(GfxActiveMember::None), depth_stencil{} {}

		GfxClearValue(float r, float g, float b, float a)
			: active_member(GfxActiveMember::Color), color(r, g, b, a)
		{
		}

		GfxClearValue(float(&_color)[4])
			: active_member(GfxActiveMember::Color), color{ _color }
		{
		}

		GfxClearValue(GfxClearColor const& color)
			: active_member(GfxActiveMember::Color), color(color)
		{}

		GfxClearValue(float depth, uint8 stencil)
			: active_member(GfxActiveMember::DepthStencil), depth_stencil(depth, stencil)
		{}
		GfxClearValue(GfxClearDepthStencil const& depth_stencil)
			: active_member(GfxActiveMember::DepthStencil), depth_stencil(depth_stencil)
		{}

		GfxClearValue(GfxClearValue const& other)
			: active_member(other.active_member), color{}, format(other.format)
		{
			if (active_member == GfxActiveMember::Color) color = other.color;
			else if (active_member == GfxActiveMember::DepthStencil) depth_stencil = other.depth_stencil;
		}

		GfxClearValue& operator=(GfxClearValue const& other)
		{
			if (this == &other) return *this;
			active_member = other.active_member;
			format = other.format;
			if (active_member == GfxActiveMember::Color) color = other.color;
			else if (active_member == GfxActiveMember::DepthStencil) depth_stencil = other.depth_stencil;
			return *this;
		}

		bool operator==(GfxClearValue const& other) const
		{
			if (active_member != other.active_member) return false;
			else if (active_member == GfxActiveMember::Color)
			{
				return color == other.color;
			}
			else return depth_stencil.depth == other.depth_stencil.depth
				&& depth_stencil.stencil == other.depth_stencil.stencil;
		}

		GfxActiveMember active_member;
		GfxFormat format = GfxFormat::UNKNOWN;
		union
		{
			GfxClearColor color;
			GfxClearDepthStencil depth_stencil;
		};
	};

	class GfxDevice;
}
