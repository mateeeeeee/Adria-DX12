#include "d3dx12_pipeline_state_stream.h"
#include "GfxPipelineState.h"
#include "GfxDevice.h"
#include "GfxStates.h"
#include "GfxShader.h"
#include "Rendering/ShaderCache.h"
#include "Core/Defines.h"

namespace adria
{

	GfxPipelineState::operator ID3D12PipelineState* () const
	{
		return pso.Get();
	}

	GraphicsPipelineState::GraphicsPipelineState(GfxDevice* gfx, GraphicsPipelineStateDesc const& desc) : GfxPipelineState(gfx, GfxPipelineStateType::Graphics), desc(desc)
	{
		Create(desc);
		event_handle = ShaderCache::GetShaderRecompiledEvent().AddMember(&GraphicsPipelineState::OnShaderRecompiled, *this);
	}

	GraphicsPipelineState::~GraphicsPipelineState()
	{
		ShaderCache::GetShaderRecompiledEvent().Remove(event_handle);
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

	ComputePipelineState::ComputePipelineState(GfxDevice* gfx, ComputePipelineStateDesc const& desc) : GfxPipelineState(gfx, GfxPipelineStateType::Compute), desc(desc)
	{
		Create(desc);
		event_handle = ShaderCache::GetShaderRecompiledEvent().AddMember(&ComputePipelineState::OnShaderRecompiled, *this);
	}

	ComputePipelineState::~ComputePipelineState()
	{
		ShaderCache::GetShaderRecompiledEvent().Remove(event_handle);
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

	MeshShaderPipelineState::MeshShaderPipelineState(GfxDevice* gfx, MeshShaderPipelineStateDesc const& desc) : GfxPipelineState(gfx, GfxPipelineStateType::MeshShader), desc(desc)
	{
		Create(desc);
		event_handle = ShaderCache::GetShaderRecompiledEvent().AddMember(&MeshShaderPipelineState::OnShaderRecompiled, *this);
	}

	MeshShaderPipelineState::~MeshShaderPipelineState()
	{
		ShaderCache::GetShaderRecompiledEvent().Remove(event_handle);
	}

	void MeshShaderPipelineState::OnShaderRecompiled(GfxShaderID s)
	{
		if (s == desc.AS || s == desc.MS || s == desc.PS) Create(desc);
	}

	void MeshShaderPipelineState::Create(MeshShaderPipelineStateDesc const& desc)
	{
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC _desc = {};

		_desc.pRootSignature = gfx->GetCommonRootSignature();
		_desc.AS = ShaderCache::GetShader(desc.AS);
		_desc.MS = ShaderCache::GetShader(desc.MS);
		_desc.PS = ShaderCache::GetShader(desc.PS);
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

		auto pso_stream = CD3DX12_PIPELINE_MESH_STATE_STREAM(_desc);
		D3D12_PIPELINE_STATE_STREAM_DESC stream_desc{};
		stream_desc.pPipelineStateSubobjectStream = &pso_stream;
		stream_desc.SizeInBytes = sizeof(pso_stream);

		BREAK_IF_FAILED(gfx->GetDevice()->CreatePipelineState(&stream_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
	}

}