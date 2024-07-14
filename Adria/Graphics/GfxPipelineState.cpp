#include "d3dx12_pipeline_state_stream.h"
#include "GfxPipelineState.h"
#include "GfxDevice.h"
#include "GfxStates.h"
#include "GfxShader.h"
#include "GfxResourceCommon.h"
#include "Rendering/ShaderManager.h"
#include "Utilities/HashUtil.h"

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
		inline void ConvertInputLayout(GfxInputLayout const& input_layout, std::vector<D3D12_INPUT_ELEMENT_DESC>& element_descs)
		{
			element_descs.resize(input_layout.elements.size());
			for (uint32 i = 0; i < element_descs.size(); ++i)
			{
				GfxInputLayout::GfxInputElement const& element = input_layout.elements[i];
				D3D12_INPUT_ELEMENT_DESC desc{};
				desc.AlignedByteOffset = element.aligned_byte_offset;
				desc.Format = ConvertGfxFormat(element.format);
				desc.InputSlot = element.input_slot;
				switch (element.input_slot_class)
				{
				case GfxInputClassification::PerVertexData:
					desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
					break;
				case GfxInputClassification::PerInstanceData:
					desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
					break;
				}
				desc.InstanceDataStepRate = 0;
				if (desc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA) desc.InstanceDataStepRate = 1;
				desc.SemanticIndex = element.semantic_index;
				desc.SemanticName = element.semantic_name.c_str();
				element_descs[i] = desc;
			}
		}
	}

	GfxPipelineState::operator ID3D12PipelineState* () const
	{
		return pso.Get();
	}

	GfxGraphicsPipelineState::GfxGraphicsPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc) : GfxPipelineState(gfx, GfxPipelineStateType::Graphics), desc(desc)
	{
		Create(desc);
		event_handle = ShaderManager::GetShaderRecompiledEvent().AddMember(&GfxGraphicsPipelineState::OnShaderRecompiled, *this);
	}
	GfxGraphicsPipelineState::~GfxGraphicsPipelineState()
	{
		ShaderManager::GetShaderRecompiledEvent().Remove(event_handle);
	}
	void GfxGraphicsPipelineState::OnShaderRecompiled(GfxShaderKey const& s)
	{
		GfxShaderKey shaders[] = { desc.VS, desc.PS, desc.GS, desc.HS, desc.DS };
		for (uint64 i = 0; i < ARRAYSIZE(shaders); ++i)
		{
			if (s == shaders[i])
			{
				Create(desc);
				return;
			}
		}
	}
	void GfxGraphicsPipelineState::Create(GfxGraphicsPipelineStateDesc const& desc)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12_desc{};
		d3d12_desc.pRootSignature = gfx->GetCommonRootSignature();
		d3d12_desc.VS = GetGfxShader(desc.VS);
		d3d12_desc.PS = GetGfxShader(desc.PS);
		d3d12_desc.GS = GetGfxShader(desc.GS);
		d3d12_desc.HS = GetGfxShader(desc.HS);
		d3d12_desc.DS = GetGfxShader(desc.DS);
		std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_descs;
		ConvertInputLayout(desc.input_layout, input_element_descs);
		d3d12_desc.InputLayout = { .pInputElementDescs = input_element_descs.data(), .NumElements = (UINT)input_element_descs.size() };
		d3d12_desc.BlendState = ConvertBlendDesc(desc.blend_state);
		d3d12_desc.RasterizerState = ConvertRasterizerDesc(desc.rasterizer_state);
		d3d12_desc.DepthStencilState = ConvertDepthStencilDesc(desc.depth_state);
		d3d12_desc.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1, .Quality = 0 };
		d3d12_desc.DSVFormat = ConvertGfxFormat(desc.dsv_format);
		d3d12_desc.NumRenderTargets = desc.num_render_targets;
		for (uint64 i = 0; i < ARRAYSIZE(d3d12_desc.RTVFormats); ++i)
		{
			d3d12_desc.RTVFormats[i] = ConvertGfxFormat(desc.rtv_formats[i]);
		}
		d3d12_desc.PrimitiveTopologyType = ConvertPrimitiveTopologyType(desc.topology_type);
		d3d12_desc.SampleMask = desc.sample_mask;
		if (d3d12_desc.DSVFormat == DXGI_FORMAT_UNKNOWN) d3d12_desc.DepthStencilState.DepthEnable = false;
		HRESULT hr = gfx->GetDevice()->CreateGraphicsPipelineState(&d3d12_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf()));
		GFX_CHECK_HR(hr);
	}

	GfxComputePipelineState::GfxComputePipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc) : GfxPipelineState(gfx, GfxPipelineStateType::Compute), desc(desc)
	{
		Create(desc);
		event_handle = ShaderManager::GetShaderRecompiledEvent().AddMember(&GfxComputePipelineState::OnShaderRecompiled, *this);
	}
	GfxComputePipelineState::~GfxComputePipelineState()
	{
		ShaderManager::GetShaderRecompiledEvent().Remove(event_handle);
	}
	void GfxComputePipelineState::OnShaderRecompiled(GfxShaderKey const& s)
	{
		if (s == desc.CS) Create(desc);
	}
	void GfxComputePipelineState::Create(GfxComputePipelineStateDesc const& desc)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12_desc{};
		d3d12_desc.pRootSignature = gfx->GetCommonRootSignature();
		d3d12_desc.CS = GetGfxShader(desc.CS);
		GFX_CHECK_HR(gfx->GetDevice()->CreateComputePipelineState(&d3d12_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
	}

	GfxMeshShaderPipelineState::GfxMeshShaderPipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc) : GfxPipelineState(gfx, GfxPipelineStateType::MeshShader), desc(desc)
	{
		Create(desc);
		event_handle = ShaderManager::GetShaderRecompiledEvent().AddMember(&GfxMeshShaderPipelineState::OnShaderRecompiled, *this);
	}
	GfxMeshShaderPipelineState::~GfxMeshShaderPipelineState()
	{
		ShaderManager::GetShaderRecompiledEvent().Remove(event_handle);
	}
	void GfxMeshShaderPipelineState::OnShaderRecompiled(GfxShaderKey const& s)
	{
		if (s == desc.AS || s == desc.MS || s == desc.PS) Create(desc);
	}
	void GfxMeshShaderPipelineState::Create(GfxMeshShaderPipelineStateDesc const& desc)
	{
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC d3d12_desc{};

		d3d12_desc.pRootSignature = gfx->GetCommonRootSignature();
		d3d12_desc.AS = GetGfxShader(desc.AS);
		d3d12_desc.MS = GetGfxShader(desc.MS);
		d3d12_desc.PS = GetGfxShader(desc.PS);
		d3d12_desc.BlendState = ConvertBlendDesc(desc.blend_state);
		d3d12_desc.RasterizerState = ConvertRasterizerDesc(desc.rasterizer_state);
		d3d12_desc.DepthStencilState = ConvertDepthStencilDesc(desc.depth_state);
		d3d12_desc.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1, .Quality = 0 };
		d3d12_desc.DSVFormat = ConvertGfxFormat(desc.dsv_format);
		d3d12_desc.NumRenderTargets = desc.num_render_targets;
		for (uint32 i = 0; i < ARRAYSIZE(d3d12_desc.RTVFormats); ++i)
		{
			d3d12_desc.RTVFormats[i] = ConvertGfxFormat(desc.rtv_formats[i]);
		}
		d3d12_desc.PrimitiveTopologyType = ConvertPrimitiveTopologyType(desc.topology_type);
		d3d12_desc.SampleMask = desc.sample_mask;
		if (d3d12_desc.DSVFormat == DXGI_FORMAT_UNKNOWN) d3d12_desc.DepthStencilState.DepthEnable = false;

		auto pso_stream = CD3DX12_PIPELINE_MESH_STATE_STREAM(d3d12_desc);
		D3D12_PIPELINE_STATE_STREAM_DESC stream_desc{};
		stream_desc.pPipelineStateSubobjectStream = &pso_stream;
		stream_desc.SizeInBytes = sizeof(pso_stream);

		GFX_CHECK_HR(gfx->GetDevice()->CreatePipelineState(&stream_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
	}

}