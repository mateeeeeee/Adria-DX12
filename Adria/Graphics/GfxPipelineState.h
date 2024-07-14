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

	struct GfxGraphicsPipelineStateDesc
	{
		GfxRasterizerState rasterizer_state{};
		GfxBlendState blend_state{};
		GfxDepthStencilState depth_state{};
		GfxPrimitiveTopologyType topology_type = GfxPrimitiveTopologyType::Triangle;
		uint32 num_render_targets = 0;
		GfxFormat rtv_formats[8];
		GfxFormat dsv_format = GfxFormat::UNKNOWN;
		GfxInputLayout input_layout;
		GfxRootSignatureID root_signature = GfxRootSignatureID::Common;
		GfxShaderKey VS;
		GfxShaderKey PS;
		GfxShaderKey DS;
		GfxShaderKey HS;
		GfxShaderKey GS;
		uint32 sample_mask = UINT_MAX;
	};
	class GfxGraphicsPipelineState : public GfxPipelineState
	{
	public:
		GfxGraphicsPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc);
		~GfxGraphicsPipelineState();

	private:
		GfxGraphicsPipelineStateDesc desc;
	private:
		void OnShaderRecompiled(GfxShaderKey const&);
		void Create(GfxGraphicsPipelineStateDesc const& desc);
	};

	struct GfxComputePipelineStateDesc
	{
		GfxRootSignatureID root_signature = GfxRootSignatureID::Common;
		GfxShaderKey CS;
	};
	class GfxComputePipelineState : public GfxPipelineState
	{
	public:
		GfxComputePipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc);
		~GfxComputePipelineState();

	private:
		GfxComputePipelineStateDesc desc;
		
	private:
		void OnShaderRecompiled(GfxShaderKey const&);
		void Create(GfxComputePipelineStateDesc const& desc);
	};

	struct GfxMeshShaderPipelineStateDesc
	{
		GfxRasterizerState rasterizer_state{};
		GfxBlendState blend_state{};
		GfxDepthStencilState depth_state{};
		GfxPrimitiveTopologyType topology_type = GfxPrimitiveTopologyType::Triangle;
		uint32 num_render_targets = 0;
		GfxFormat rtv_formats[8];
		GfxFormat dsv_format = GfxFormat::UNKNOWN;
		GfxRootSignatureID root_signature = GfxRootSignatureID::Common;
		GfxShaderKey AS;
		GfxShaderKey MS;
		GfxShaderKey PS;
		uint32 sample_mask = UINT_MAX;
	};
	class GfxMeshShaderPipelineState : public GfxPipelineState
	{
	public:
		GfxMeshShaderPipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc);
		~GfxMeshShaderPipelineState();

	private:
		GfxMeshShaderPipelineStateDesc desc;

	private:
		void OnShaderRecompiled(GfxShaderKey const&);
		void Create(GfxMeshShaderPipelineStateDesc const& desc);
	};
}