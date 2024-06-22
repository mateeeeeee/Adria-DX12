#pragma once
#include "GfxFormat.h"
#include "D3D12MemAlloc.h"
#include "Utilities/EnumUtil.h"
#include "Utilities/StringUtil.h"
#include "Utilities/Ref.h"
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
	template <>
	struct EnumBitmaskOperators<GfxBindFlag>
	{
		static constexpr bool enable = true;
	};

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
	template <>
	struct EnumBitmaskOperators<GfxTextureMiscFlag>
	{
		static constexpr bool enable = true;
	};

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
	template <>
	struct EnumBitmaskOperators<GfxBufferMiscFlag>
	{
		static constexpr bool enable = true;
	};

	enum class GfxBarrierState : uint64
	{
		None = 0,
		Common = 1 << 0,
		Present = Common,
		RTV = 1 << 1,
		DSV = 1 << 2,
		DSV_ReadOnly = 1 << 3,
		VertexSRV = 1 << 4,
		PixelSRV = 1 << 5,
		ComputeSRV = 1 << 6,
		VertexUAV = 1 << 7,
		PixelUAV = 1 << 8,
		ComputeUAV = 1 << 9,
		ClearUAV = 1 << 10,
		CopyDst = 1 << 11,
		CopySrc = 1 << 12,
		ShadingRate = 1 << 13,
		IndexBuffer = 1 << 14,
		IndirectArgs = 1 << 15,
		ASRead = 1 << 16,
		ASWrite = 1 << 17,
		Discard = 1 << 18,

		AllVertex = VertexSRV | VertexUAV,
		AllPixel = PixelSRV | PixelUAV,
		AllCompute = ComputeSRV | ComputeUAV,
		AllSRV = VertexSRV | PixelSRV | ComputeSRV,
		AllUAV = VertexUAV | PixelUAV | ComputeUAV,
		AllDSV = DSV | DSV_ReadOnly,
		AllCopy = CopyDst | CopySrc,
		AllAS = ASRead | ASWrite,
		GenericRead = CopySrc | AllSRV
	};
	template <>
	struct EnumBitmaskOperators<GfxBarrierState>
	{
		static constexpr bool enable = true;
	};

	inline D3D12_BARRIER_SYNC ToD3D12BarrierSync(GfxBarrierState flags)
	{
		D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;
		bool discard = HasAnyFlag(flags, GfxBarrierState::Discard);
		if (!discard && HasAnyFlag(flags,GfxBarrierState::ClearUAV)) sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
		if (HasAnyFlag(flags, GfxBarrierState::Present))		sync |= D3D12_BARRIER_SYNC_ALL;
		if (HasAnyFlag(flags, GfxBarrierState::RTV))			sync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		if (HasAnyFlag(flags, GfxBarrierState::AllDSV))			sync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if (HasAnyFlag(flags, GfxBarrierState::AllVertex))		sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		if (HasAnyFlag(flags, GfxBarrierState::AllPixel))		sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasAnyFlag(flags, GfxBarrierState::AllCompute))		sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		if (HasAnyFlag(flags, GfxBarrierState::AllCopy))		sync |= D3D12_BARRIER_SYNC_COPY;
		if (HasAnyFlag(flags, GfxBarrierState::ShadingRate))	sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasAnyFlag(flags, GfxBarrierState::IndexBuffer))	sync |= D3D12_BARRIER_SYNC_INDEX_INPUT;
		if (HasAnyFlag(flags, GfxBarrierState::IndirectArgs))	sync |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
		if (HasAnyFlag(flags, GfxBarrierState::AllAS))			sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		return sync;
	}
	inline D3D12_BARRIER_LAYOUT ToD3D12BarrierLayout(GfxBarrierState flags)
	{
		if (HasAnyFlag(flags, GfxBarrierState::Discard))		return D3D12_BARRIER_LAYOUT_UNDEFINED;
		if (HasAnyFlag(flags, GfxBarrierState::Present))		return D3D12_BARRIER_LAYOUT_PRESENT;
		if (HasAnyFlag(flags, GfxBarrierState::RTV))			return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		if (HasAnyFlag(flags, GfxBarrierState::DSV))            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		if (HasAnyFlag(flags, GfxBarrierState::DSV_ReadOnly))   return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, GfxBarrierState::AllSRV))			return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		if (HasAnyFlag(flags, GfxBarrierState::AllUAV))			return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasAnyFlag(flags, GfxBarrierState::ClearUAV))		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasAnyFlag(flags, GfxBarrierState::CopyDst))		return D3D12_BARRIER_LAYOUT_COPY_DEST;
		if (HasAnyFlag(flags, GfxBarrierState::CopySrc))		return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		if (HasAnyFlag(flags, GfxBarrierState::ShadingRate))	return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
		ADRIA_UNREACHABLE();
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	inline D3D12_BARRIER_ACCESS ToD3D12BarrierAccess(GfxBarrierState flags)
	{
		if (HasAnyFlag(flags, GfxBarrierState::Discard)) return D3D12_BARRIER_ACCESS_NO_ACCESS;
		D3D12_BARRIER_ACCESS access = D3D12_BARRIER_ACCESS_COMMON;
		if (HasAnyFlag(flags, GfxBarrierState::RTV))             access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if (HasAnyFlag(flags, GfxBarrierState::DSV))             access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if (HasAnyFlag(flags, GfxBarrierState::DSV_ReadOnly))    access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, GfxBarrierState::AllSRV))          access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		if (HasAnyFlag(flags, GfxBarrierState::AllUAV))          access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasAnyFlag(flags, GfxBarrierState::ClearUAV))        access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasAnyFlag(flags, GfxBarrierState::CopyDst))         access |= D3D12_BARRIER_ACCESS_COPY_DEST;
		if (HasAnyFlag(flags, GfxBarrierState::CopySrc))         access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if (HasAnyFlag(flags, GfxBarrierState::ShadingRate))     access |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
		if (HasAnyFlag(flags, GfxBarrierState::IndexBuffer))     access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
		if (HasAnyFlag(flags, GfxBarrierState::IndirectArgs))    access |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
		if (HasAnyFlag(flags, GfxBarrierState::ASRead))          access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		if (HasAnyFlag(flags, GfxBarrierState::ASWrite))         access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
		return access;
	}

	inline bool CompareByLayout(GfxBarrierState flags1, GfxBarrierState flags2)
	{
		return ToD3D12BarrierLayout(flags1) == ToD3D12BarrierLayout(flags2);
	}

	inline constexpr std::string ConvertBarrierFlagsToString(GfxBarrierState flags)
	{
		std::string resource_state_string = "";
		if (!resource_state_string.empty()) resource_state_string.pop_back();
		return resource_state_string.empty() ? "Common" : resource_state_string;
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
