#pragma once
#include <d3d12.h>
#include "../Core/Definitions.h"

namespace adria
{
	enum class EPrimitiveTopology : uint8
	{
		Undefined,
		TriangleList,
		TriangleStrip,
		PointList,
		LineList,
		LineStrip,
		PatchList,
	};
	enum class EComparisonFunc : uint8
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
	enum class EDepthWriteMask : uint8
	{
		Zero,	
		All,	
	};
	enum class EStencilOp : uint8
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
	enum class EBlend : uint8
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
	enum class EBlendOp : uint8
	{
		Add,
		Subtract,
		RevSubtract,
		Min,
		Max,
	};
	enum class EFillMode : uint8
	{
		Wireframe,
		Solid,
	};
	enum class ECullMode : uint8
	{
		None,
		Front,
		Back,
	};
	enum class EColorWrite
	{
		Disable = 0,
		EnableRed = 1 << 0,
		EnableGreen = 1 << 1,
		EnableBlue = 1 << 2,
		EnableAlpha = 1 << 3,
		EnableAll = ~0,
	};

	struct RasterizerState
	{
		EFillMode fill_mode = EFillMode::Solid;
		ECullMode cull_mode = ECullMode::Back;
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
	struct DepthStencilState
	{
		bool depth_enable = true;
		EDepthWriteMask depth_write_mask = EDepthWriteMask::All;
		EComparisonFunc depth_func = EComparisonFunc::Less;
		bool stencil_enable = false;
		uint8 stencil_read_mask = 0xff;
		uint8 stencil_write_mask = 0xff;
		struct DepthStencilOp
		{
			EStencilOp stencil_fail_op = EStencilOp::Keep;
			EStencilOp stencil_depth_fail_op = EStencilOp::Keep;
			EStencilOp stencil_pass_op = EStencilOp::Keep;
			EComparisonFunc stencil_func = EComparisonFunc::Always;
		};
		DepthStencilOp front_face{};
		DepthStencilOp back_face{};
	};
	struct BlendState
	{
		bool alpha_to_coverage_enable = false;
		bool independent_blend_enable = false;
		struct RenderTargetBlendState
		{
			bool blend_enable = false;
			EBlend src_blend = EBlend::One;
			EBlend dest_blend = EBlend::Zero;
			EBlendOp blend_op = EBlendOp::Add;
			EBlend src_blend_alpha = EBlend::One;
			EBlend dest_blend_alpha = EBlend::Zero;
			EBlendOp blend_op_alpha = EBlendOp::Add;
			EColorWrite render_target_write_mask = EColorWrite::EnableAll;
		};
		RenderTargetBlendState render_target[8];
	};

	D3D12_RASTERIZER_DESC ConvertRasterizerDesc(RasterizerState);
	D3D12_DEPTH_STENCIL_DESC ConvertDepthStencilDesc(DepthStencilState);
	D3D12_BLEND_DESC ConvertBlendDesc(BlendState);
	inline constexpr D3D_PRIMITIVE_TOPOLOGY ConvertPrimitiveTopology(EPrimitiveTopology topology, uint32_t controlPoints = 0)
	{
		switch (topology)
		{
		case EPrimitiveTopology::PointList:
			return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case EPrimitiveTopology::LineList:
			return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case EPrimitiveTopology::LineStrip:
			return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case EPrimitiveTopology::TriangleList:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case EPrimitiveTopology::TriangleStrip:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case EPrimitiveTopology::PatchList:
			if (controlPoints == 0 || controlPoints > 32) return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
			else return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoints - 1));
		default:
			return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		}
	}

}