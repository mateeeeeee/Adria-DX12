#include "GraphicsStates.h"
#include "d3dx12.h"
#include "../Utilities/EnumUtil.h"

namespace adria
{
	namespace
	{
		constexpr D3D12_FILL_MODE ConvertFillMode(EFillMode value)
		{
			switch (value)
			{
			case EFillMode::Wireframe:
				return D3D12_FILL_MODE_WIREFRAME;
				break;
			case EFillMode::Solid:
				return D3D12_FILL_MODE_SOLID;
				break;
			default:
				break;
			}
			return D3D12_FILL_MODE_WIREFRAME;
		}
		constexpr D3D12_CULL_MODE ConvertCullMode(ECullMode value)
		{
			switch (value)
			{
			case ECullMode::None:
				return D3D12_CULL_MODE_NONE;
				break;
			case ECullMode::Front:
				return D3D12_CULL_MODE_FRONT;
				break;
			case ECullMode::Back:
				return D3D12_CULL_MODE_BACK;
				break;
			default:
				break;
			}
			return D3D12_CULL_MODE_NONE;
		}
		constexpr D3D12_DEPTH_WRITE_MASK ConvertDepthWriteMask(EDepthWriteMask value)
		{
			switch (value)
			{
			case EDepthWriteMask::Zero:
				return D3D12_DEPTH_WRITE_MASK_ZERO;
				break;
			case EDepthWriteMask::All:
				return D3D12_DEPTH_WRITE_MASK_ALL;
				break;
			default:
				break;
			}
			return D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		constexpr D3D12_STENCIL_OP ConvertStencilOp(EStencilOp value)
		{
			switch (value)
			{
			case EStencilOp::Keep:
				return D3D12_STENCIL_OP_KEEP;
				break;
			case EStencilOp::Zero:
				return D3D12_STENCIL_OP_ZERO;
				break;
			case EStencilOp::Replace:
				return D3D12_STENCIL_OP_REPLACE;
				break;
			case EStencilOp::IncrSat:
				return D3D12_STENCIL_OP_INCR_SAT;
				break;
			case EStencilOp::DecrSat:
				return D3D12_STENCIL_OP_DECR_SAT;
				break;
			case EStencilOp::Invert:
				return D3D12_STENCIL_OP_INVERT;
				break;
			case EStencilOp::Incr:
				return D3D12_STENCIL_OP_INCR;
				break;
			case EStencilOp::Decr:
				return D3D12_STENCIL_OP_DECR;
				break;
			default:
				break;
			}
			return D3D12_STENCIL_OP_KEEP;
		}
		constexpr D3D12_BLEND ConvertBlend(EBlend value)
		{
			switch (value)
			{
			case EBlend::Zero:
				return D3D12_BLEND_ZERO;
			case EBlend::One:
				return D3D12_BLEND_ONE;
			case EBlend::SrcColor:
				return D3D12_BLEND_SRC_COLOR;
			case EBlend::InvSrcColor:
				return D3D12_BLEND_INV_SRC_COLOR;
			case EBlend::SrcAlpha:
				return D3D12_BLEND_SRC_ALPHA;
			case EBlend::InvSrcAlpha:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case EBlend::DstAlpha:
				return D3D12_BLEND_DEST_ALPHA;
			case EBlend::InvDstAlpha:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case EBlend::DstColor:
				return D3D12_BLEND_DEST_COLOR;
			case EBlend::InvDstColor:
				return D3D12_BLEND_INV_DEST_COLOR;
			case EBlend::SrcAlphaSat:
				return D3D12_BLEND_SRC_ALPHA_SAT;
			case EBlend::BlendFactor:
				return D3D12_BLEND_BLEND_FACTOR;
			case EBlend::InvBlendFactor:
				return D3D12_BLEND_INV_BLEND_FACTOR;
			case EBlend::Src1Color:
				return D3D12_BLEND_SRC1_COLOR;
			case EBlend::InvSrc1Color:
				return D3D12_BLEND_INV_SRC1_COLOR;
			case EBlend::Src1Alpha:
				return D3D12_BLEND_SRC1_ALPHA;
			case EBlend::InvSrc1Alpha:
				return D3D12_BLEND_INV_SRC1_ALPHA;
			default:
				return D3D12_BLEND_ZERO;
			}
		}
		constexpr D3D12_BLEND ConvertAlphaBlend(EBlend value)
		{
			switch (value)
			{
			case EBlend::SrcColor:
				return D3D12_BLEND_SRC_ALPHA;
			case EBlend::InvSrcColor:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case EBlend::DstColor:
				return D3D12_BLEND_DEST_ALPHA;
			case EBlend::InvDstColor:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case EBlend::Src1Color:
				return D3D12_BLEND_SRC1_ALPHA;
			case EBlend::InvSrc1Color:
				return D3D12_BLEND_INV_SRC1_ALPHA;
			default:
				return ConvertBlend(value);
			}
		}
		constexpr D3D12_BLEND_OP ConvertBlendOp(EBlendOp value)
		{
			switch (value)
			{
			case EBlendOp::Add:
				return D3D12_BLEND_OP_ADD;
			case EBlendOp::Subtract:
				return D3D12_BLEND_OP_SUBTRACT;
			case EBlendOp::RevSubtract:
				return D3D12_BLEND_OP_REV_SUBTRACT;
			case EBlendOp::Min:
				return D3D12_BLEND_OP_MIN;
			case EBlendOp::Max:
				return D3D12_BLEND_OP_MAX;
			default:
				return D3D12_BLEND_OP_ADD;
			}
		}
		constexpr D3D12_COMPARISON_FUNC ConvertComparisonFunc(EComparisonFunc value)
		{
			switch (value)
			{
			case EComparisonFunc::Never:
				return D3D12_COMPARISON_FUNC_NEVER;
				break;
			case EComparisonFunc::Less:
				return D3D12_COMPARISON_FUNC_LESS;
				break;
			case EComparisonFunc::Equal:
				return D3D12_COMPARISON_FUNC_EQUAL;
				break;
			case EComparisonFunc::LessEqual:
				return D3D12_COMPARISON_FUNC_LESS_EQUAL;
				break;
			case EComparisonFunc::Greater:
				return D3D12_COMPARISON_FUNC_GREATER;
				break;
			case EComparisonFunc::NotEqual:
				return D3D12_COMPARISON_FUNC_NOT_EQUAL;
				break;
			case EComparisonFunc::GreaterEqual:
				return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
				break;
			case EComparisonFunc::Always:
				return D3D12_COMPARISON_FUNC_ALWAYS;
				break;
			default:
				break;
			}
			return D3D12_COMPARISON_FUNC_NEVER;
		}
		constexpr uint32 ParseColorWriteMask(EColorWrite value)
		{
			uint32 _flag = 0;
			if (value == EColorWrite::EnableAll)
			{
				return D3D12_COLOR_WRITE_ENABLE_ALL;
			}
			else
			{
				if (HasAnyFlag(value, EColorWrite::EnableRed))
					_flag |= D3D12_COLOR_WRITE_ENABLE_RED;
				if (HasAnyFlag(value, EColorWrite::EnableGreen))
					_flag |= D3D12_COLOR_WRITE_ENABLE_GREEN;
				if (HasAnyFlag(value, EColorWrite::EnableBlue))
					_flag |= D3D12_COLOR_WRITE_ENABLE_BLUE;
				if (HasAnyFlag(value, EColorWrite::EnableAlpha))
					_flag |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
			}
			return _flag;
		}
	}

	D3D12_RASTERIZER_DESC ConvertRasterizerDesc(RasterizerState _rs)
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
	D3D12_DEPTH_STENCIL_DESC ConvertDepthStencilDesc(DepthStencilState _dss)
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
	D3D12_BLEND_DESC ConvertBlendDesc(BlendState _bd)
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
