#include "GfxStates.h"
#include "d3dx12.h"
#include "Utilities/EnumUtil.h"

namespace adria
{
	namespace
	{
		constexpr D3D12_FILL_MODE ConvertFillMode(GfxFillMode value)
		{
			switch (value)
			{
			case GfxFillMode::Wireframe:
				return D3D12_FILL_MODE_WIREFRAME;
				break;
			case GfxFillMode::Solid:
				return D3D12_FILL_MODE_SOLID;
				break;
			default:
				break;
			}
			return D3D12_FILL_MODE_WIREFRAME;
		}
		constexpr D3D12_CULL_MODE ConvertCullMode(GfxCullMode value)
		{
			switch (value)
			{
			case GfxCullMode::None:
				return D3D12_CULL_MODE_NONE;
				break;
			case GfxCullMode::Front:
				return D3D12_CULL_MODE_FRONT;
				break;
			case GfxCullMode::Back:
				return D3D12_CULL_MODE_BACK;
				break;
			default:
				break;
			}
			return D3D12_CULL_MODE_NONE;
		}
		constexpr D3D12_DEPTH_WRITE_MASK ConvertDepthWriteMask(GfxDepthWriteMask value)
		{
			switch (value)
			{
			case GfxDepthWriteMask::Zero:
				return D3D12_DEPTH_WRITE_MASK_ZERO;
				break;
			case GfxDepthWriteMask::All:
				return D3D12_DEPTH_WRITE_MASK_ALL;
				break;
			default:
				break;
			}
			return D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		constexpr D3D12_STENCIL_OP ConvertStencilOp(GfxStencilOp value)
		{
			switch (value)
			{
			case GfxStencilOp::Keep:
				return D3D12_STENCIL_OP_KEEP;
				break;
			case GfxStencilOp::Zero:
				return D3D12_STENCIL_OP_ZERO;
				break;
			case GfxStencilOp::Replace:
				return D3D12_STENCIL_OP_REPLACE;
				break;
			case GfxStencilOp::IncrSat:
				return D3D12_STENCIL_OP_INCR_SAT;
				break;
			case GfxStencilOp::DecrSat:
				return D3D12_STENCIL_OP_DECR_SAT;
				break;
			case GfxStencilOp::Invert:
				return D3D12_STENCIL_OP_INVERT;
				break;
			case GfxStencilOp::Incr:
				return D3D12_STENCIL_OP_INCR;
				break;
			case GfxStencilOp::Decr:
				return D3D12_STENCIL_OP_DECR;
				break;
			default:
				break;
			}
			return D3D12_STENCIL_OP_KEEP;
		}
		constexpr D3D12_BLEND ConvertBlend(GfxBlend value)
		{
			switch (value)
			{
			case GfxBlend::Zero:
				return D3D12_BLEND_ZERO;
			case GfxBlend::One:
				return D3D12_BLEND_ONE;
			case GfxBlend::SrcColor:
				return D3D12_BLEND_SRC_COLOR;
			case GfxBlend::InvSrcColor:
				return D3D12_BLEND_INV_SRC_COLOR;
			case GfxBlend::SrcAlpha:
				return D3D12_BLEND_SRC_ALPHA;
			case GfxBlend::InvSrcAlpha:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case GfxBlend::DstAlpha:
				return D3D12_BLEND_DEST_ALPHA;
			case GfxBlend::InvDstAlpha:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case GfxBlend::DstColor:
				return D3D12_BLEND_DEST_COLOR;
			case GfxBlend::InvDstColor:
				return D3D12_BLEND_INV_DEST_COLOR;
			case GfxBlend::SrcAlphaSat:
				return D3D12_BLEND_SRC_ALPHA_SAT;
			case GfxBlend::BlendFactor:
				return D3D12_BLEND_BLEND_FACTOR;
			case GfxBlend::InvBlendFactor:
				return D3D12_BLEND_INV_BLEND_FACTOR;
			case GfxBlend::Src1Color:
				return D3D12_BLEND_SRC1_COLOR;
			case GfxBlend::InvSrc1Color:
				return D3D12_BLEND_INV_SRC1_COLOR;
			case GfxBlend::Src1Alpha:
				return D3D12_BLEND_SRC1_ALPHA;
			case GfxBlend::InvSrc1Alpha:
				return D3D12_BLEND_INV_SRC1_ALPHA;
			default:
				return D3D12_BLEND_ZERO;
			}
		}
		constexpr D3D12_BLEND ConvertAlphaBlend(GfxBlend value)
		{
			switch (value)
			{
			case GfxBlend::SrcColor:
				return D3D12_BLEND_SRC_ALPHA;
			case GfxBlend::InvSrcColor:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case GfxBlend::DstColor:
				return D3D12_BLEND_DEST_ALPHA;
			case GfxBlend::InvDstColor:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case GfxBlend::Src1Color:
				return D3D12_BLEND_SRC1_ALPHA;
			case GfxBlend::InvSrc1Color:
				return D3D12_BLEND_INV_SRC1_ALPHA;
			default:
				return ConvertBlend(value);
			}
		}
		constexpr D3D12_BLEND_OP ConvertBlendOp(GfxBlendOp value)
		{
			switch (value)
			{
			case GfxBlendOp::Add:
				return D3D12_BLEND_OP_ADD;
			case GfxBlendOp::Subtract:
				return D3D12_BLEND_OP_SUBTRACT;
			case GfxBlendOp::RevSubtract:
				return D3D12_BLEND_OP_REV_SUBTRACT;
			case GfxBlendOp::Min:
				return D3D12_BLEND_OP_MIN;
			case GfxBlendOp::Max:
				return D3D12_BLEND_OP_MAX;
			default:
				return D3D12_BLEND_OP_ADD;
			}
		}
		constexpr D3D12_COMPARISON_FUNC ConvertComparisonFunc(GfxComparisonFunc value)
		{
			switch (value)
			{
			case GfxComparisonFunc::Never:
				return D3D12_COMPARISON_FUNC_NEVER;
				break;
			case GfxComparisonFunc::Less:
				return D3D12_COMPARISON_FUNC_LESS;
				break;
			case GfxComparisonFunc::Equal:
				return D3D12_COMPARISON_FUNC_EQUAL;
				break;
			case GfxComparisonFunc::LessEqual:
				return D3D12_COMPARISON_FUNC_LESS_EQUAL;
				break;
			case GfxComparisonFunc::Greater:
				return D3D12_COMPARISON_FUNC_GREATER;
				break;
			case GfxComparisonFunc::NotEqual:
				return D3D12_COMPARISON_FUNC_NOT_EQUAL;
				break;
			case GfxComparisonFunc::GreaterEqual:
				return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
				break;
			case GfxComparisonFunc::Always:
				return D3D12_COMPARISON_FUNC_ALWAYS;
				break;
			default:
				break;
			}
			return D3D12_COMPARISON_FUNC_NEVER;
		}
		constexpr uint32 ParseColorWriteMask(GfxColorWrite value)
		{
			uint32 _flag = 0;
			if (value == GfxColorWrite::EnableAll)
			{
				return D3D12_COLOR_WRITE_ENABLE_ALL;
			}
			else
			{
				if (HasAnyFlag(value, GfxColorWrite::EnableRed))
					_flag |= D3D12_COLOR_WRITE_ENABLE_RED;
				if (HasAnyFlag(value, GfxColorWrite::EnableGreen))
					_flag |= D3D12_COLOR_WRITE_ENABLE_GREEN;
				if (HasAnyFlag(value, GfxColorWrite::EnableBlue))
					_flag |= D3D12_COLOR_WRITE_ENABLE_BLUE;
				if (HasAnyFlag(value, GfxColorWrite::EnableAlpha))
					_flag |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
			}
			return _flag;
		}
	}

	D3D12_RASTERIZER_DESC ConvertRasterizerDesc(GfxRasterizerState _rs)
	{
		CD3DX12_RASTERIZER_DESC rs{};
		rs.FillMode = ConvertFillMode(_rs.fill_mode);
		rs.CullMode = ConvertCullMode(_rs.cull_mode);
		rs.FrontCounterClockwise = _rs.front_counter_clockwise;
		rs.DepthBias = _rs.depth_bias;
		rs.DepthBiasClamp = _rs.depth_bias_clamp;
		rs.SlopeScaledDepthBias = _rs.slope_scaled_depth_bias;
		rs.DepthClipEnable = _rs.depth_clip_enable;
		rs.MultisampleEnable = _rs.multisample_enable;
		rs.AntialiasedLineEnable = _rs.antialiased_line_enable;
		rs.ConservativeRaster = _rs.conservative_rasterization_enable ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		rs.ForcedSampleCount = _rs.forced_sample_count;
		return rs;
	}
	D3D12_DEPTH_STENCIL_DESC ConvertDepthStencilDesc(GfxDepthStencilState _dss)
	{
		CD3DX12_DEPTH_STENCIL_DESC dss{};
		dss.DepthEnable = _dss.depth_enable;
		dss.DepthWriteMask = ConvertDepthWriteMask(_dss.depth_write_mask);
		dss.DepthFunc = ConvertComparisonFunc(_dss.depth_func);
		dss.StencilEnable = _dss.stencil_enable;
		dss.StencilReadMask = _dss.stencil_read_mask;
		dss.StencilWriteMask = _dss.stencil_write_mask;
		dss.FrontFace.StencilDepthFailOp = ConvertStencilOp(_dss.front_face.stencil_depth_fail_op);
		dss.FrontFace.StencilFailOp = ConvertStencilOp(_dss.front_face.stencil_fail_op);
		dss.FrontFace.StencilFunc = ConvertComparisonFunc(_dss.front_face.stencil_func);
		dss.FrontFace.StencilPassOp = ConvertStencilOp(_dss.front_face.stencil_pass_op);
		dss.BackFace.StencilDepthFailOp = ConvertStencilOp(_dss.back_face.stencil_depth_fail_op);
		dss.BackFace.StencilFailOp = ConvertStencilOp(_dss.back_face.stencil_fail_op);
		dss.BackFace.StencilFunc = ConvertComparisonFunc(_dss.back_face.stencil_func);
		dss.BackFace.StencilPassOp = ConvertStencilOp(_dss.back_face.stencil_pass_op);
		return dss;
	}
	D3D12_BLEND_DESC ConvertBlendDesc(GfxBlendState _bd)
	{
		CD3DX12_BLEND_DESC bd{};
		bd.AlphaToCoverageEnable = _bd.alpha_to_coverage_enable;
		bd.IndependentBlendEnable = _bd.independent_blend_enable;
		for (int32 i = 0; i < 8; ++i)
		{
			bd.RenderTarget[i].BlendEnable = _bd.render_target[i].blend_enable;
			bd.RenderTarget[i].SrcBlend = ConvertBlend(_bd.render_target[i].src_blend);
			bd.RenderTarget[i].DestBlend = ConvertBlend(_bd.render_target[i].dest_blend);
			bd.RenderTarget[i].BlendOp = ConvertBlendOp(_bd.render_target[i].blend_op);
			bd.RenderTarget[i].SrcBlendAlpha = ConvertAlphaBlend(_bd.render_target[i].src_blend_alpha);
			bd.RenderTarget[i].DestBlendAlpha = ConvertAlphaBlend(_bd.render_target[i].dest_blend_alpha);
			bd.RenderTarget[i].BlendOpAlpha = ConvertBlendOp(_bd.render_target[i].blend_op_alpha);
			bd.RenderTarget[i].RenderTargetWriteMask = ParseColorWriteMask(_bd.render_target[i].render_target_write_mask);
		}
		return bd;
	}
}
