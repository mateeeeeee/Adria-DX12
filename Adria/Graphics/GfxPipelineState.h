#pragma once
#include "GfxStates.h"
#include "GfxShaderKey.h"
#include "GfxInputLayout.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class GfxDevice;

	enum class GfxRootSignatureID : uint8
	{
		Invalid,
		Common
	};

	enum class GfxPipelineStateType : uint8
	{
		Graphics,
		Compute,
		MeshShader
	};

	class GfxPipelineState
	{
	public:
		operator ID3D12PipelineState*() const;
		GfxPipelineStateType GetType() const { return type; }

	protected:
		GfxPipelineState(GfxDevice* gfx, GfxPipelineStateType type) : gfx(gfx), type(type) {}
	protected:
		GfxDevice* gfx;
		Ref<ID3D12PipelineState> pso;
		GfxPipelineStateType type;
		DelegateHandle event_handle;
	};

	struct GraphicsPipelineStateDesc
	{
		GfxRasterizerState rasterizer_state{};
		GfxBlendState blend_state{};
		GfxDepthStencilState depth_state{};
		GfxPrimitiveTopologyType topology_type = GfxPrimitiveTopologyType::Triangle;
		uint32 num_render_targets = 0;
		GfxFormat rtv_formats[8];
		GfxFormat dsv_format = GfxFormat::UNKNOWN;
		GfxInputLayout input_layout;
		GfxRootSignatureID root_signature = GfxRootSignatureID::Invalid;
		GfxShaderKey VS;
		GfxShaderKey PS;
		GfxShaderKey DS;
		GfxShaderKey HS;
		GfxShaderKey GS;
		uint32 sample_mask = UINT_MAX;
	};
	class GraphicsPipelineState : public GfxPipelineState
	{
	public:
		GraphicsPipelineState(GfxDevice* gfx, GraphicsPipelineStateDesc const& desc);
		~GraphicsPipelineState();

	private:
		GraphicsPipelineStateDesc desc;
	private:
		void OnShaderRecompiled(GfxShaderKey const&);
		void Create(GraphicsPipelineStateDesc const& desc);
	};

	struct ComputePipelineStateDesc
	{
		GfxRootSignatureID root_signature;
		GfxShaderKey CS;
	};
	class ComputePipelineState : public GfxPipelineState
	{
	public:
		ComputePipelineState(GfxDevice* gfx, ComputePipelineStateDesc const& desc);
		~ComputePipelineState();

	private:
		ComputePipelineStateDesc desc;
		
	private:
		void OnShaderRecompiled(GfxShaderKey const&);
		void Create(ComputePipelineStateDesc const& desc);
	};

	struct MeshShaderPipelineStateDesc
	{
		GfxRasterizerState rasterizer_state{};
		GfxBlendState blend_state{};
		GfxDepthStencilState depth_state{};
		GfxPrimitiveTopologyType topology_type = GfxPrimitiveTopologyType::Triangle;
		uint32 num_render_targets = 0;
		GfxFormat rtv_formats[8];
		GfxFormat dsv_format = GfxFormat::UNKNOWN;
		GfxRootSignatureID root_signature = GfxRootSignatureID::Invalid;
		GfxShaderKey AS;
		GfxShaderKey MS;
		GfxShaderKey PS;
		uint32 sample_mask = UINT_MAX;
	};
	class MeshShaderPipelineState : public GfxPipelineState
	{
	public:
		MeshShaderPipelineState(GfxDevice* gfx, MeshShaderPipelineStateDesc const& desc);
		~MeshShaderPipelineState();

	private:
		MeshShaderPipelineStateDesc desc;

	private:
		void OnShaderRecompiled(GfxShaderKey const&);
		void Create(MeshShaderPipelineStateDesc const& desc);
	};
}