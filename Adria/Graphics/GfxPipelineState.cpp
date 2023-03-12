#include "GfxPipelineState.h"
#include "GfxDevice.h"
#include "GfxStates.h"
#include "GfxShader.h"
#include "../Rendering/ShaderCache.h"
#include "../Core/Macros.h"

namespace adria
{

	GraphicsPipelineState::GraphicsPipelineState(GfxDevice* gfx, GraphicsPipelineStateDesc const& desc) : desc(desc), gfx(gfx)
	{
		Create(desc);
		event_handle = ShaderCache::GetShaderRecompiledEvent().AddMember(&GraphicsPipelineState::OnShaderRecompiled, *this);
	}

	GraphicsPipelineState::~GraphicsPipelineState()
	{
		ShaderCache::GetShaderRecompiledEvent().Remove(event_handle);
	}

	GraphicsPipelineState::operator ID3D12PipelineState* () const
	{
		return pso.Get();
	}

	void GraphicsPipelineState::OnShaderRecompiled(GfxShaderID s)
	{
		GfxShaderID shaders[] = { desc.VS, desc.PS, desc.GS, desc.HS, desc.DS };
		for (size_t i = 0; i < ARRAYSIZE(shaders); ++i)
		{
			if (s == shaders[i])
			{
				Create(desc);
				return;
			}
		}
	}

	void GraphicsPipelineState::Create(GraphicsPipelineStateDesc const& desc)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc{};
		_desc.pRootSignature = gfx->GetCommonRootSignature();
		_desc.VS = ShaderCache::GetShader(desc.VS);
		_desc.PS = ShaderCache::GetShader(desc.PS);
		_desc.GS = ShaderCache::GetShader(desc.GS);
		_desc.HS = ShaderCache::GetShader(desc.HS);
		_desc.DS = ShaderCache::GetShader(desc.DS);
		std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_descs;
		ConvertInputLayout(desc.input_layout, input_element_descs);
		_desc.InputLayout = { .pInputElementDescs = input_element_descs.data(), .NumElements = (UINT)input_element_descs.size() };
		_desc.BlendState = ConvertBlendDesc(desc.blend_state);
		_desc.RasterizerState = ConvertRasterizerDesc(desc.rasterizer_state);
		_desc.DepthStencilState = ConvertDepthStencilDesc(desc.depth_state);
		_desc.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1, .Quality = 0 };
		_desc.DSVFormat = ConvertGfxFormat(desc.dsv_format);
		_desc.NumRenderTargets = desc.num_render_targets;
		for (size_t i = 0; i < ARRAYSIZE(_desc.RTVFormats); ++i)
		{
			_desc.RTVFormats[i] = ConvertGfxFormat(desc.rtv_formats[i]);
		}
		_desc.PrimitiveTopologyType = ConvertPrimitiveTopologyType(desc.topology_type);
		_desc.SampleMask = desc.sample_mask;
		if (_desc.DSVFormat == DXGI_FORMAT_UNKNOWN) _desc.DepthStencilState.DepthEnable = false;
		BREAK_IF_FAILED(gfx->GetDevice()->CreateGraphicsPipelineState(&_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
	}

	ComputePipelineState::ComputePipelineState(GfxDevice* gfx, ComputePipelineStateDesc const& desc) : gfx(gfx), desc(desc)
	{
		Create(desc);
		event_handle = ShaderCache::GetShaderRecompiledEvent().AddMember(&ComputePipelineState::OnShaderRecompiled, *this);
	}

	ComputePipelineState::~ComputePipelineState()
	{
		ShaderCache::GetShaderRecompiledEvent().Remove(event_handle);
	}

	ComputePipelineState::operator ID3D12PipelineState* () const
	{
		return pso.Get();
	}

	void ComputePipelineState::OnShaderRecompiled(GfxShaderID s)
	{
		if (s == desc.CS) Create(desc);
	}

	void ComputePipelineState::Create(ComputePipelineStateDesc const& desc)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC _desc{};
		_desc.pRootSignature = gfx->GetCommonRootSignature();
		_desc.CS = ShaderCache::GetShader(desc.CS);
		BREAK_IF_FAILED(gfx->GetDevice()->CreateComputePipelineState(&_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
	}

}