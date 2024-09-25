#pragma once
#include "GfxFormat.h"
#include "D3D12MemAlloc.h"
#include "Utilities/EnumUtil.h"
#include "Utilities/StringUtil.h"
#include "Utilities/Ref.h"
#include "Utilities/Releasable.h"

namespace adria
{
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
	ENABLE_ENUM_BIT_OPERATORS(GfxBindFlag);

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
	ENABLE_ENUM_BIT_OPERATORS(GfxTextureMiscFlag);

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
	ENABLE_ENUM_BIT_OPERATORS(GfxBufferMiscFlag);

	enum class GfxResourceState : uint64
	{
		None = 0,
		Common = 1 << 0,
		Present = 1 << 1,
		RTV = 1 << 2,
		DSV = 1 << 3,
		DSV_ReadOnly = 1 << 4,
		VertexSRV = 1 << 5,
		PixelSRV = 1 << 6,
		ComputeSRV = 1 << 7,
		VertexUAV = 1 << 8,
		PixelUAV = 1 << 9,
		ComputeUAV = 1 << 10,
		ClearUAV = 1 << 11,
		CopyDst = 1 << 12,
		CopySrc = 1 << 13,
		ShadingRate = 1 << 14,
		IndexBuffer = 1 << 15,
		IndirectArgs = 1 << 16,
		ASRead = 1 << 17,
		ASWrite = 1 << 18,
		Discard = 1 << 19,

		AllVertex = VertexSRV | VertexUAV,
		AllPixel = PixelSRV | PixelUAV,
		AllCompute = ComputeSRV | ComputeUAV,
		AllSRV = VertexSRV | PixelSRV | ComputeSRV,
		AllUAV = VertexUAV | PixelUAV | ComputeUAV,
		AllDSV = DSV | DSV_ReadOnly,
		AllCopy = CopyDst | CopySrc,
		AllAS = ASRead | ASWrite,
		GenericRead = CopySrc | AllSRV,
		GenericWrite = CopyDst | AllUAV,
		AllShading = AllSRV | AllUAV | ShadingRate | ASRead
	};
	ENABLE_ENUM_BIT_OPERATORS(GfxResourceState);

	inline D3D12_BARRIER_SYNC ToD3D12BarrierSync(GfxResourceState flags)
	{
		using enum GfxResourceState;

		D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;
		bool const discard = HasFlag(flags, Discard);
		if (!discard && HasFlag(flags, ClearUAV)) sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;

		if (HasFlag(flags, Present))		sync |= D3D12_BARRIER_SYNC_ALL;
		if (HasFlag(flags, Common))			sync |= D3D12_BARRIER_SYNC_ALL;
		if (HasFlag(flags, RTV))			sync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		if (HasAnyFlag(flags, AllDSV))		sync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if (HasAnyFlag(flags, AllVertex))	sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		if (HasAnyFlag(flags, AllPixel))	sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasAnyFlag(flags, AllCompute))	sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		if (HasAnyFlag(flags, AllCopy))		sync |= D3D12_BARRIER_SYNC_COPY;
		if (HasFlag(flags, ShadingRate))	sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasFlag(flags, IndexBuffer))	sync |= D3D12_BARRIER_SYNC_INDEX_INPUT;
		if (HasFlag(flags, IndirectArgs))	sync |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
		if (HasAnyFlag(flags, AllAS))		sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		return sync;
	}
	inline D3D12_BARRIER_LAYOUT ToD3D12BarrierLayout(GfxResourceState flags)
	{
		using enum GfxResourceState;

		if(HasFlag(flags, CopySrc) && HasAnyFlag(flags, AllSRV)) return D3D12_BARRIER_LAYOUT_GENERIC_READ;
		if(HasFlag(flags, CopyDst) && HasAnyFlag(flags, AllUAV)) return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
		if (HasFlag(flags, DSV_ReadOnly) && (HasAnyFlag(flags, AllSRV) || HasFlag(flags, CopySrc))) return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
		if (HasFlag(flags, Discard))		return D3D12_BARRIER_LAYOUT_UNDEFINED;
		if (HasFlag(flags, Present))		return D3D12_BARRIER_LAYOUT_PRESENT;
		if (HasFlag(flags, RTV))			return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		if (HasFlag(flags, DSV))            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		if (HasFlag(flags, DSV_ReadOnly))   return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, AllSRV))		return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		if (HasAnyFlag(flags, AllUAV))		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasFlag(flags, ClearUAV))		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasFlag(flags, CopyDst))		return D3D12_BARRIER_LAYOUT_COPY_DEST;
		if (HasFlag(flags, CopySrc))		return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		if (HasFlag(flags, ShadingRate))	return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
		ADRIA_UNREACHABLE();
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	inline D3D12_BARRIER_ACCESS ToD3D12BarrierAccess(GfxResourceState flags)
	{
		using enum GfxResourceState;
		if (HasFlag(flags, Discard)) return D3D12_BARRIER_ACCESS_NO_ACCESS;

		D3D12_BARRIER_ACCESS access = D3D12_BARRIER_ACCESS_COMMON;
		if (HasFlag(flags, RTV))             access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if (HasFlag(flags, DSV))             access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if (HasFlag(flags, DSV_ReadOnly))    access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, AllSRV))       access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		if (HasAnyFlag(flags, AllUAV))       access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasFlag(flags, ClearUAV))        access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasFlag(flags, CopyDst))         access |= D3D12_BARRIER_ACCESS_COPY_DEST;
		if (HasFlag(flags, CopySrc))         access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if (HasFlag(flags, ShadingRate))     access |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
		if (HasFlag(flags, IndexBuffer))     access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
		if (HasFlag(flags, IndirectArgs))    access |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
		if (HasFlag(flags, ASRead))          access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		if (HasFlag(flags, ASWrite))         access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
		return access;
	}
	inline constexpr D3D12_RESOURCE_STATES ToD3D12LegacyResourceState(GfxResourceState state)
	{
		using enum GfxResourceState;

		D3D12_RESOURCE_STATES api_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasAnyFlag(state, IndexBuffer))		api_state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (HasFlag(state, RTV))				api_state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (HasAnyFlag(state, AllUAV))			api_state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (HasFlag(state, DSV))				api_state |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if (HasFlag(state, DSV_ReadOnly))		api_state |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (HasAnyFlag(state, ComputeSRV | VertexSRV)) api_state |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (HasFlag(state, PixelSRV))			api_state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (HasFlag(state, IndirectArgs))		api_state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (HasFlag(state, CopyDst))			api_state |= D3D12_RESOURCE_STATE_COPY_DEST;
		if (HasFlag(state, CopySrc))			api_state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if (HasFlag(state, Present))			api_state |= D3D12_RESOURCE_STATE_PRESENT;
		if (HasAnyFlag(state, AllAS))			api_state |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		return api_state;
	}

	inline bool CompareByLayout(GfxResourceState flags1, GfxResourceState flags2)
	{
		return ToD3D12BarrierLayout(flags1) == ToD3D12BarrierLayout(flags2);
	}
	inline constexpr std::string ConvertBarrierFlagsToString(GfxResourceState flags)
	{
		std::string resource_state_string = "";
		if (!resource_state_string.empty()) resource_state_string.pop_back();
		return resource_state_string.empty() ? "Common" : resource_state_string;
	}

	class GfxDevice;
}
