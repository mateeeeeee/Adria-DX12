#pragma once
#include  <d3d12.h>
#include "Enums.h"
#include "../Graphics/GraphicsStates.h"
#include "../Graphics/Shader.h"
#include "../Graphics/InputLayout.h"
#include "../Graphics/ResourceCommon.h"
#include "../Events/Delegate.h"

namespace adria
{
	class GraphicsDevice;

	struct GraphicsPipelineStateDesc
	{
		RasterizerState rasterizer_state{};
		BlendState blend_state{};
		DepthStencilState depth_state{};
		EPrimitiveTopologyType topology_type = EPrimitiveTopologyType::Triangle;
		uint32 num_render_targets = 0;
		EFormat rtv_formats[8];
		EFormat dsv_format = EFormat::UNKNOWN;
		InputLayout input_layout;
		ERootSignature root_signature = ERootSignature::Invalid;
		EShaderId VS = ShaderId_Invalid;
		EShaderId PS = ShaderId_Invalid;
		EShaderId DS = ShaderId_Invalid;
		EShaderId HS = ShaderId_Invalid;
		EShaderId GS = ShaderId_Invalid;
		uint32 sample_mask = UINT_MAX;
	};

	class GraphicsPipelineState
	{
	public:
		GraphicsPipelineState(GraphicsDevice* gfx, GraphicsPipelineStateDesc const& desc);
		~GraphicsPipelineState();

		operator ID3D12PipelineState*() const;

	private:
		GraphicsDevice* gfx;
		ArcPtr<ID3D12PipelineState> pso;
		GraphicsPipelineStateDesc desc;
		DelegateHandle event_handle;
	private:
		void OnShaderRecompiled(EShaderId s);
		void Create(GraphicsPipelineStateDesc const& desc);
	};

	struct ComputePipelineStateDesc
	{
		ERootSignature root_signature;
		EShaderId CS;
	};

	class ComputePipelineState
	{
	public:
		ComputePipelineState(GraphicsDevice* gfx, ComputePipelineStateDesc const& desc);
		~ComputePipelineState();

		operator ID3D12PipelineState*() const;

	private:
		GraphicsDevice* gfx;
		ArcPtr<ID3D12PipelineState> pso;
		ComputePipelineStateDesc desc;
		DelegateHandle event_handle;

	private:
		void OnShaderRecompiled(EShaderId s);
		void Create(ComputePipelineStateDesc const& desc);
	};
}