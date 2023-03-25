#pragma once
#include <d3d12.h>
#include "../Core/CoreTypes.h"

namespace adria
{

	enum class GfxPrimitiveTopologyType : uint8
	{
		Undefined,
		Point,
		Line,
		Triangle,
		Patch
	};
	enum class GfxPrimitiveTopology : uint8
	{
		Undefined,
		TriangleList,
		TriangleStrip,
		PointList,
		LineList,
		LineStrip,
		PatchList,
	};
	enum class GfxComparisonFunc : uint8
	{
		Never,
		Less,
		Equal,
		LessEqual,
		Greater,
		NotEqual,
		GreaterEqual,
		Always,
	};
	enum class GfxDepthWriteMask : uint8
	{
		Zero,	
		All,	
	};
	enum class GfxStencilOp : uint8
	{
		Keep,
		Zero,
		Replace,
		IncrSat,
		DecrSat,
		Invert,
		Incr,
		Decr,
	};
	enum class GfxBlend : uint8
	{
		Zero,
		One,
		SrcColor,
		InvSrcColor,
		SrcAlpha,
		InvSrcAlpha,
		DstAlpha,
		InvDstAlpha,
		DstColor,
		InvDstColor,
		SrcAlphaSat,
		BlendFactor,
		InvBlendFactor,
		Src1Color,
		InvSrc1Color,
		Src1Alpha,
		InvSrc1Alpha,
	};
	enum class GfxBlendOp : uint8
	{
		Add,
		Subtract,
		RevSubtract,
		Min,
		Max,
	};
	enum class GfxFillMode : uint8
	{
		Wireframe,
		Solid,
	};
	enum class GfxCullMode : uint8
	{
		None,
		Front,
		Back,
	};
	enum class GfxColorWrite
	{
		Disable = 0,
		EnableRed = 1 << 0,
		EnableGreen = 1 << 1,
		EnableBlue = 1 << 2,
		EnableAlpha = 1 << 3,
		EnableAll = ~0,
	};

	struct GfxRasterizerState
	{
		GfxFillMode fill_mode = GfxFillMode::Solid;
		GfxCullMode cull_mode = GfxCullMode::Back;
		bool front_counter_clockwise = false;
		int32 depth_bias = 0;
		float depth_bias_clamp = 0.0f;
		float slope_scaled_depth_bias = 0.0f;
		bool depth_clip_enable = true;
		bool multisample_enable = false;
		bool antialiased_line_enable = false;
		bool conservative_rasterization_enable = false;
		uint32 forced_sample_count = 0;
	};
	struct GfxDepthStencilState
	{
		bool depth_enable = true;
		GfxDepthWriteMask depth_write_mask = GfxDepthWriteMask::All;
		GfxComparisonFunc depth_func = GfxComparisonFunc::Less;
		bool stencil_enable = false;
		uint8 stencil_read_mask = 0xff;
		uint8 stencil_write_mask = 0xff;
		struct GfxDepthStencilOp
		{
			GfxStencilOp stencil_fail_op = GfxStencilOp::Keep;
			GfxStencilOp stencil_depth_fail_op = GfxStencilOp::Keep;
			GfxStencilOp stencil_pass_op = GfxStencilOp::Keep;
			GfxComparisonFunc stencil_func = GfxComparisonFunc::Always;
		};
		GfxDepthStencilOp front_face{};
		GfxDepthStencilOp back_face{};
	};
	struct GfxBlendState
	{
		bool alpha_to_coverage_enable = false;
		bool independent_blend_enable = false;
		struct GfxRenderTargetBlendState
		{
			bool blend_enable = false;
			GfxBlend src_blend = GfxBlend::One;
			GfxBlend dest_blend = GfxBlend::Zero;
			GfxBlendOp blend_op = GfxBlendOp::Add;
			GfxBlend src_blend_alpha = GfxBlend::One;
			GfxBlend dest_blend_alpha = GfxBlend::Zero;
			GfxBlendOp blend_op_alpha = GfxBlendOp::Add;
			GfxColorWrite render_target_write_mask = GfxColorWrite::EnableAll;
		};
		GfxRenderTargetBlendState render_target[8];
	};

	D3D12_RASTERIZER_DESC ConvertRasterizerDesc(GfxRasterizerState);
	D3D12_DEPTH_STENCIL_DESC ConvertDepthStencilDesc(GfxDepthStencilState);
	D3D12_BLEND_DESC ConvertBlendDesc(GfxBlendState);
	inline constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE ConvertPrimitiveTopologyType(GfxPrimitiveTopologyType topology_type)
	{
		switch (topology_type)
		{
		case GfxPrimitiveTopologyType::Undefined:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		case GfxPrimitiveTopologyType::Point:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case GfxPrimitiveTopologyType::Line:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case GfxPrimitiveTopologyType::Triangle:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case GfxPrimitiveTopologyType::Patch:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		default:
			break;
		}
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	}
	inline constexpr D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveTopology(GfxPrimitiveTopology topology, uint32_t control_points = 0)
	{
		switch (topology)
		{
		case GfxPrimitiveTopology::PointList:
			return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case GfxPrimitiveTopology::LineList:
			return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case GfxPrimitiveTopology::LineStrip:
			return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case GfxPrimitiveTopology::TriangleList:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case GfxPrimitiveTopology::TriangleStrip:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case GfxPrimitiveTopology::PatchList:
			if (control_points == 0 || control_points > 32) return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
			else return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (control_points - 1));
		default:
			return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		}
	}
}