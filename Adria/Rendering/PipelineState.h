#pragma once
#include  <d3d12.h>
#include <wrl.h>
#include "Enums.h"
#include "../Graphics/GraphicsStates.h"
#include "../Graphics/Shader.h"
#include "../Events/Delegate.h"

namespace adria
{
	class GraphicsDevice;

	struct GraphicsPipelineStateDesc
	{
		RasterizerState rasterizer_state{};
		BlendState blend_state{};
		DepthStencilState depth_state{};
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		UINT num_render_targets = 0;
		DXGI_FORMAT rtv_formats[8];
		DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;
		InputLayout input_layout;
		ERootSignature root_signature = ERootSignature::Invalid;
		EShader VS = EShader_Invalid;
		EShader PS = EShader_Invalid;
		EShader DS = EShader_Invalid;
		EShader HS = EShader_Invalid;
		EShader GS = EShader_Invalid;
		UINT sample_mask = UINT_MAX;
	};

	class GraphicsPipelineState
	{
	public:
		GraphicsPipelineState(GraphicsDevice* gfx, GraphicsPipelineStateDesc const& desc);
		~GraphicsPipelineState();

		operator ID3D12PipelineState* () const;

	private:
		GraphicsDevice* gfx;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		GraphicsPipelineStateDesc desc;
		DelegateHandle event_handle;
	private:
		void OnShaderRecompiled(EShader s);
		void Create(GraphicsPipelineStateDesc const& desc);
	};

	struct ComputePipelineStateDesc
	{
		ERootSignature root_signature;
		EShader CS;
	};

	class ComputePipelineState
	{
	public:
		ComputePipelineState(GraphicsDevice* gfx, ComputePipelineStateDesc const& desc);
		~ComputePipelineState();

		operator ID3D12PipelineState* () const;

	private:
		GraphicsDevice* gfx;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		ComputePipelineStateDesc desc;
		DelegateHandle event_handle;

	private:
		void OnShaderRecompiled(EShader s);
		void Create(ComputePipelineStateDesc const& desc);
	};
}