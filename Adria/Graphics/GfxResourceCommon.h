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

	enum GfxBarrierFlag : uint64
	{
		GfxBarrierFlag_Present = 1 << 0,
		GfxBarrierFlag_RTV = 1 << 1,
		GfxBarrierFlag_DSV = 1 << 2,
		GfxBarrierFlag_DSV_ReadOnly = 1 << 3,
		GfxBarrierFlag_VertexSRV = 1 << 4,
		GfxBarrierFlag_PixelSRV = 1 << 5,
		GfxBarrierFlag_ComputeSRV = 1 << 6,
		GfxBarrierFlag_VertexUAV = 1 << 7,
		GfxBarrierFlag_PixelUAV = 1 << 8,
		GfxBarrierFlag_ComputeUAV = 1 << 9,
		GfxBarrierFlag_ClearUAV = 1 << 10,
		GfxBarrierFlag_CopyDst = 1 << 11,
		GfxBarrierFlag_CopySrc = 1 << 12,
		GfxBarrierFlag_ShadingRate = 1 << 13,
		GfxBarrierFlag_IndexBuffer = 1 << 14,
		GfxBarrierFlag_IndirectArgs = 1 << 15,
		GfxBarrierFlag_ASRead = 1 << 16,
		GfxBarrierFlag_ASWrite = 1 << 17,
		GfxBarrierFlag_Discard = 1 << 18,

		GfxBarrierFlag_AllVertex = VertexSRV | VertexUAV,
		GfxBarrierFlag_AllPixel = PixelSRV | PixelUAV,
		GfxBarrierFlag_AllCompute = ComputeSRV | ComputeUAV,
		GfxBarrierFlag_AllSRV = VertexSRV | PixelSRV | ComputeSRV,
		GfxBarrierFlag_AllUAV = VertexUAV | PixelUAV | ComputeUAV,
		GfxBarrierFlag_AllDSV = DSV | DSV_ReadOnly,
		GfxBarrierFlag_AllCopy = CopyDst | CopySrc,
		GfxBarrierFlag_AllAS = ASRead | ASWrite
	};
	using GfxBarrierFlags = uint64;

	inline D3D12_BARRIER_SYNC ToD3D12BarrierSync(GfxBarrierFlags flags)
	{
		D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;
		bool discard = (flags & GfxBarrierFlag_Discard) != 0;
		if (!discard && (flags & GfxBarrierFlag_ClearUAV)) sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
		if (flags & GfxBarrierFlag_Present)         sync |= D3D12_BARRIER_SYNC_ALL;
		if (flags & GfxBarrierFlag_RTV)				sync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		if (flags & GfxBarrierFlag_AllDSV)			sync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if (flags & GfxBarrierFlag_AllVertex)		sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		if (flags & GfxBarrierFlag_AllPixel)		sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (flags & GfxBarrierFlag_AllCompute)		sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		if (flags & GfxBarrierFlag_AllCopy)			sync |= D3D12_BARRIER_SYNC_COPY;
		if (flags & GfxBarrierFlag_ShadingRate)		sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (flags & GfxBarrierFlag_IndexBuffer)		sync |= D3D12_BARRIER_SYNC_INDEX_INPUT;
		if (flags & GfxBarrierFlag_IndirectArgs)	sync |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
		if (flags & GfxBarrierFlag_AllAS)			sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		return sync;
	}
	inline D3D12_BARRIER_LAYOUT ToD3D12BarrierLayout(GfxBarrierFlags flags)
	{
		if (flags & GfxBarrierFlag_Discard)			return D3D12_BARRIER_LAYOUT_UNDEFINED;
		if (flags & GfxBarrierFlag_Present)			return D3D12_BARRIER_LAYOUT_PRESENT;
		if (flags & GfxBarrierFlag_RTV)				return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		if (flags & GfxBarrierFlag_DSV)             return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		if (flags & GfxBarrierFlag_DSV_ReadOnly)    return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		if (flags & GfxBarrierFlag_AllSRV)			return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		if (flags & GfxBarrierFlag_AllUAV)			return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (flags & GfxBarrierFlag_ClearUAV)		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (flags & GfxBarrierFlag_CopyDst)			return D3D12_BARRIER_LAYOUT_COPY_DEST;
		if (flags & GfxBarrierFlag_CopySrc)			return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		if (flags & GfxBarrierFlag_ShadingRate)		return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
		ADRIA_UNREACHABLE();
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	inline D3D12_BARRIER_ACCESS ToD3D12BarrierAccess(GfxBarrierFlags flags)
	{
		if (flags & GfxBarrierFlag_Discard) return D3D12_BARRIER_ACCESS_NO_ACCESS;
		D3D12_BARRIER_ACCESS access = D3D12_BARRIER_ACCESS_COMMON;
		if (flags & GfxBarrierFlag_RTV)             access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if (flags & GfxBarrierFlag_DSV)             access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if (flags & GfxBarrierFlag_DSV_ReadOnly)    access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		if (flags & GfxBarrierFlag_AllSRV)          access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		if (flags & GfxBarrierFlag_AllUAV)          access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (flags & GfxBarrierFlag_ClearUAV)        access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (flags & GfxBarrierFlag_CopyDst)         access |= D3D12_BARRIER_ACCESS_COPY_DEST;
		if (flags & GfxBarrierFlag_CopySrc)         access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if (flags & GfxBarrierFlag_ShadingRate)     access |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
		if (flags & GfxBarrierFlag_IndexBuffer)     access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
		if (flags & GfxBarrierFlag_IndirectArgs)    access |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
		if (flags & GfxBarrierFlag_ASRead)          access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		if (flags & GfxBarrierFlag_ASWrite)         access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
		return access;
	}

	inline bool CompareByLayout(GfxBarrierFlags flags1, GfxBarrierFlags flags2)
	{
		return ToD3D12BarrierLayout(flags1) == ToD3D12BarrierLayout(flags2);
	}

#pragma region old_resource_state

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
	template <>
	struct EnumBitmaskOperators<GfxResourceState>
	{
		static constexpr bool enable = true;
	};

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
	inline constexpr D3D12_RESOURCE_STATES ConvertGfxResourceStateToD3D12(GfxResourceState state)
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
	inline constexpr std::string ConvertGfxResourceStateToString(GfxResourceState state)
	{
		std::string resource_state_string = "";
		if (HasAnyFlag(state, GfxResourceState::VertexAndConstantBuffer)) resource_state_string += "D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |";
		if (HasAnyFlag(state, GfxResourceState::IndexBuffer)) resource_state_string += "D3D12_RESOURCE_STATE_INDEX_BUFFER |";
		if (HasAnyFlag(state, GfxResourceState::RenderTarget)) resource_state_string += "D3D12_RESOURCE_STATE_RENDER_TARGET |";
		if (HasAnyFlag(state, GfxResourceState::UnorderedAccess)) resource_state_string += "D3D12_RESOURCE_STATE_UNORDERED_ACCESS |";
		if (HasAnyFlag(state, GfxResourceState::DepthWrite)) resource_state_string += "D3D12_RESOURCE_STATE_DEPTH_WRITE |";
		if (HasAnyFlag(state, GfxResourceState::DepthRead)) resource_state_string += "D3D12_RESOURCE_STATE_DEPTH_READ |";
		if (HasAnyFlag(state, GfxResourceState::NonPixelShaderResource)) resource_state_string += "D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |";
		if (HasAnyFlag(state, GfxResourceState::PixelShaderResource)) resource_state_string += "D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |";
		if (HasAnyFlag(state, GfxResourceState::IndirectArgument)) resource_state_string += "D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT |";
		if (HasAnyFlag(state, GfxResourceState::CopyDest)) resource_state_string += "D3D12_RESOURCE_STATE_COPY_DEST |";
		if (HasAnyFlag(state, GfxResourceState::CopySource)) resource_state_string += "D3D12_RESOURCE_STATE_COPY_SOURCE |";
		if (HasAnyFlag(state, GfxResourceState::Present)) resource_state_string += "D3D12_RESOURCE_STATE_PRESENT |";
		if (HasAnyFlag(state, GfxResourceState::RaytracingAccelerationStructure)) resource_state_string += "D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE |";
		if (!resource_state_string.empty()) resource_state_string.pop_back();
		return resource_state_string.empty() ? "Common" : resource_state_string;
	}

#pragma endregion

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
