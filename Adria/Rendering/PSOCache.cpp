#include "PSOCache.h"
#include "ShaderManager.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Graphics/ShaderCompiler.h"
#include "../Logging/Logger.h"
#include "../Utilities/Timer.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	namespace
	{
		GraphicsDevice* gfx;
		HashMap<EPipelineState, GraphicsPipelineState> gfx_pso_map;
		HashMap<EPipelineState, ComputePipelineState>  compute_pso_map;

		void CreateAllPSOs()
		{
			GraphicsPipelineStateDesc gfx_desc{};
			{
				gfx_desc.root_signature = ERootSignature::Particles_Shading;
				gfx_desc.VS = VS_Particles;
				gfx_desc.PS = PS_Particles;
			}

			graphics_pso_desc.InputLayout = { nullptr, 0u };
			graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Shading].Get();
			graphics_pso_desc.VS = GetShader(VS_Particles);
			graphics_pso_desc.PS = GetShader(PS_Particles);
			graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
			graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			graphics_pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
			graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
			graphics_pso_desc.SampleMask = UINT_MAX;
			graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			graphics_pso_desc.NumRenderTargets = 1;
			graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			graphics_pso_desc.SampleDesc.Count = 1;
			graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
			BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Shading])));
			AddDependency(EPipelineStateObject::Particles_Shading, { VS_Particles, PS_Particles }, graphics_pso_desc);

		}
	}

	void PSOCache::Initialize(GraphicsDevice* _gfx)
	{
		gfx = _gfx;
	}

	void PSOCache::Destroy()
	{

	}

	ComputePipelineState& PSOCache::GetComputePipelineState(EPipelineState)
	{

	}

	GraphicsPipelineState& PSOCache::GetGraphicsPipelineState(EPipelineState)
	{

	}

}

