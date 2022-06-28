#include <memory>
#include <array>
#include <string_view>
#include <execution>
#include <variant>
#include "PipelineState.h"
#include "ShaderManager.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Graphics/ShaderCompiler.h"
#include "../Logging/Logger.h"
#include "../Utilities/Timer.h"
#include "../Utilities/HashMap.h"

using namespace Microsoft::WRL;

namespace adria
{
	#if 0
namespace
	{
		void CreateAllPSOs()
		{
			ID3D12Device* device = gfx->GetDevice();
			D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pso_desc{};
			InputLayout input_layout;
			{
				
				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(ShaderManager::GetShader(VS_Skybox), input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Skybox].Get();
				graphics_pso_desc.VS = GetShader(VS_Skybox);
				graphics_pso_desc.PS = GetShader(PS_Skybox);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Skybox])));
				AddDependency(EPipelineStateObject::Skybox, { VS_Skybox, PS_Skybox }, graphics_pso_desc);

				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Sky].Get();
				graphics_pso_desc.PS = GetShader(PS_UniformColorSky);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::UniformColorSky])));
				AddDependency(EPipelineStateObject::UniformColorSky, { VS_Skybox, PS_UniformColorSky }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_HosekWilkieSky);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::HosekWilkieSky])));
				AddDependency(EPipelineStateObject::HosekWilkieSky, { VS_Skybox, PS_HosekWilkieSky }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::ToneMap].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_ToneMap);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ToneMap])));
				AddDependency(EPipelineStateObject::ToneMap, { VS_ScreenQuad, PS_ToneMap }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::FXAA].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Fxaa);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FXAA])));
				AddDependency(EPipelineStateObject::FXAA, { VS_ScreenQuad, PS_Fxaa }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::TAA].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Taa);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::TAA])));
				AddDependency(EPipelineStateObject::TAA, { VS_ScreenQuad, PS_Taa }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_GBufferPBR], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::GbufferPBR].Get();
				graphics_pso_desc.VS = GetShader(VS_GBufferPBR);
				graphics_pso_desc.PS = GetShader(PS_GBufferPBR);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = static_cast<uint32>(3);
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GbufferPBR])));
				AddDependency(EPipelineStateObject::GbufferPBR, { VS_GBufferPBR, PS_GBufferPBR }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0u };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::AmbientPBR].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_AmbientPBR);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR])));
				AddDependency(EPipelineStateObject::AmbientPBR, { VS_ScreenQuad, PS_AmbientPBR }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_AmbientPBR_AO);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_AO])));
				AddDependency(EPipelineStateObject::AmbientPBR_AO, { VS_ScreenQuad, PS_AmbientPBR_AO }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_AmbientPBR_IBL);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_IBL])));
				AddDependency(EPipelineStateObject::AmbientPBR_IBL, { VS_ScreenQuad, PS_AmbientPBR_IBL }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_AmbientPBR_AO_IBL);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_AO_IBL])));
				AddDependency(EPipelineStateObject::AmbientPBR_AO_IBL, { VS_ScreenQuad, PS_AmbientPBR_AO_IBL }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0u };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::LightingPBR].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_LightingPBR);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LightingPBR])));
				AddDependency(EPipelineStateObject::LightingPBR, { VS_ScreenQuad, PS_LightingPBR }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_LightingPBR_RayTracedShadows);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LightingPBR_RayTracedShadows])));
				AddDependency(EPipelineStateObject::LightingPBR_RayTracedShadows, { VS_ScreenQuad, PS_LightingPBR_RayTracedShadows }, graphics_pso_desc);

				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::ClusteredLightingPBR].Get();
				graphics_pso_desc.PS = GetShader(PS_ClusteredLightingPBR);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusteredLightingPBR])));
				AddDependency(EPipelineStateObject::ClusteredLightingPBR, { VS_ScreenQuad, PS_ClusteredLightingPBR }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_DepthMap], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::DepthMap].Get();
				graphics_pso_desc.VS = GetShader(VS_DepthMap);
				graphics_pso_desc.PS = GetShader(PS_DepthMap);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
				graphics_pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				graphics_pso_desc.RasterizerState.DepthBias = 7500;
				graphics_pso_desc.RasterizerState.DepthBiasClamp = 0.0f;
				graphics_pso_desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
				graphics_pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 0;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DepthMap])));
				AddDependency(EPipelineStateObject::DepthMap, { VS_DepthMap, PS_DepthMap }, graphics_pso_desc);

				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_DepthMap_Transparent], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::DepthMap_Transparent].Get();
				graphics_pso_desc.VS = GetShader(VS_DepthMap_Transparent);
				graphics_pso_desc.PS = GetShader(PS_DepthMap_Transparent);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DepthMap_Transparent])));
				AddDependency(EPipelineStateObject::DepthMap_Transparent, { VS_DepthMap_Transparent, PS_DepthMap_Transparent }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0u };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Volumetric].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Volumetric_Directional);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Directional])));
				AddDependency(EPipelineStateObject::Volumetric_Directional, { VS_ScreenQuad, PS_Volumetric_Directional }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_Volumetric_DirectionalCascades);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_DirectionalCascades])));
				AddDependency(EPipelineStateObject::Volumetric_DirectionalCascades, { VS_ScreenQuad, PS_Volumetric_DirectionalCascades }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_Volumetric_Spot);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Spot])));
				AddDependency(EPipelineStateObject::Volumetric_Spot, { VS_ScreenQuad, PS_Volumetric_Spot }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_Volumetric_Point);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Point])));
				AddDependency(EPipelineStateObject::Volumetric_Point, { VS_ScreenQuad, PS_Volumetric_Point }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_Sun], input_layout);
				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Forward].Get();
				graphics_pso_desc.VS = GetShader(VS_Sun);
				graphics_pso_desc.PS = GetShader(PS_Texture);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Sun])));
				AddDependency(EPipelineStateObject::Sun, { VS_Sun, PS_Texture }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::AO].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Ssao);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::SSAO])));
				AddDependency(EPipelineStateObject::SSAO, { VS_ScreenQuad, PS_Ssao }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_Hbao);
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::HBAO])));
				AddDependency(EPipelineStateObject::HBAO, { VS_ScreenQuad, PS_Hbao }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::SSR].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Ssr);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::SSR])));
				AddDependency(EPipelineStateObject::SSR, { VS_ScreenQuad, PS_Ssr }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::GodRays].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_GodRays);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GodRays])));
				AddDependency(EPipelineStateObject::GodRays, { VS_ScreenQuad, PS_GodRays }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::LensFlare].Get();
				graphics_pso_desc.VS = GetShader(VS_LensFlare);
				graphics_pso_desc.GS = GetShader(GS_LensFlare);
				graphics_pso_desc.PS = GetShader(PS_LensFlare);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LensFlare])));
				AddDependency(EPipelineStateObject::LensFlare, { VS_LensFlare, GS_LensFlare, PS_LensFlare }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Copy].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Copy);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy])));
				AddDependency(EPipelineStateObject::Copy, { VS_ScreenQuad, PS_Copy }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy_AlphaBlend])));
				AddDependency(EPipelineStateObject::Copy_AlphaBlend, { VS_ScreenQuad, PS_Copy }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy_AdditiveBlend])));
				AddDependency(EPipelineStateObject::Copy_AdditiveBlend, { VS_ScreenQuad, PS_Copy }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Add].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Add);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add])));
				AddDependency(EPipelineStateObject::Add, { VS_ScreenQuad, PS_Add }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add_AlphaBlend])));
				AddDependency(EPipelineStateObject::Add_AlphaBlend, { VS_ScreenQuad, PS_Add }, graphics_pso_desc);

				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add_AdditiveBlend])));
				AddDependency(EPipelineStateObject::Add_AdditiveBlend, { VS_ScreenQuad, PS_Add }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Fog].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Fog);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Fog])));
				AddDependency(EPipelineStateObject::Fog, { VS_ScreenQuad, PS_Fog }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::DOF].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_Dof);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DOF])));
				AddDependency(EPipelineStateObject::DOF, { VS_ScreenQuad, PS_Dof }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Bokeh].Get();
				graphics_pso_desc.VS = GetShader(VS_Bokeh);
				graphics_pso_desc.GS = GetShader(GS_Bokeh);
				graphics_pso_desc.PS = GetShader(PS_Bokeh);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Bokeh])));
				AddDependency(EPipelineStateObject::Bokeh, { VS_Bokeh, GS_Bokeh, PS_Bokeh }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Clouds].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_VolumetricClouds);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Clouds])));
				AddDependency(EPipelineStateObject::Clouds, { VS_ScreenQuad, PS_VolumetricClouds }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::MotionBlur].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_MotionBlur);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::MotionBlur])));
				AddDependency(EPipelineStateObject::MotionBlur, { VS_ScreenQuad, PS_MotionBlur }, graphics_pso_desc);

				graphics_pso_desc = {};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::VelocityBuffer].Get();
				graphics_pso_desc.VS = GetShader(VS_ScreenQuad);
				graphics_pso_desc.PS = GetShader(PS_VelocityBuffer);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::VelocityBuffer])));
				AddDependency(EPipelineStateObject::VelocityBuffer, { VS_ScreenQuad, PS_VelocityBuffer }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_Ocean], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Ocean].Get();
				graphics_pso_desc.VS = GetShader(VS_Ocean);
				graphics_pso_desc.PS = GetShader(PS_Ocean);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = TRUE;
				graphics_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				graphics_pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				graphics_pso_desc.DepthStencilState.StencilEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Ocean])));
				AddDependency(EPipelineStateObject::Ocean, { VS_Ocean, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Ocean_Wireframe])));
				AddDependency(EPipelineStateObject::Ocean_Wireframe, { VS_Ocean, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::OceanLOD].Get();
				graphics_pso_desc.VS = GetShader(VS_OceanLOD);
				graphics_pso_desc.DS = GetShader(DS_OceanLOD);
				graphics_pso_desc.HS = GetShader(HS_OceanLOD);
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanLOD_Wireframe])));
				AddDependency(EPipelineStateObject::OceanLOD_Wireframe, { VS_OceanLOD, DS_OceanLOD, HS_OceanLOD, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanLOD])));
				AddDependency(EPipelineStateObject::OceanLOD, { VS_OceanLOD, DS_OceanLOD, HS_OceanLOD, PS_Ocean }, graphics_pso_desc);

				graphics_pso_desc = {};
				ShaderCompiler::CreateInputLayoutWithReflection(shader_map[VS_Decals], input_layout);
				graphics_pso_desc.InputLayout = input_layout;
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Decals].Get();
				graphics_pso_desc.VS = GetShader(VS_Decals);
				graphics_pso_desc.PS = GetShader(PS_Decals);
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				graphics_pso_desc.DepthStencilState.DepthEnable = FALSE;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Decals])));
				AddDependency(EPipelineStateObject::Decals, { VS_Decals, PS_Decals }, graphics_pso_desc);

				graphics_pso_desc.PS = GetShader(PS_Decals_ModifyNormals);
				graphics_pso_desc.NumRenderTargets = 2;
				graphics_pso_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Decals_ModifyNormals])));
				AddDependency(EPipelineStateObject::Decals_ModifyNormals, { VS_Decals, PS_Decals }, graphics_pso_desc);
			}

			D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc{};
			{
				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Picker].Get();
				compute_pso_desc.CS = GetShader(CS_Picker);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(pso_map[EPipelineStateObject::Picker].GetAddressOf())));
				AddDependency(EPipelineStateObject::Picker, { CS_Picker }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_InitDeadList].Get();
				compute_pso_desc.CS = GetShader(CS_ParticleInitDeadList);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_InitDeadList])));
				AddDependency(EPipelineStateObject::Particles_InitDeadList, { CS_ParticleInitDeadList }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Reset].Get();
				compute_pso_desc.CS = GetShader(CS_ParticleReset);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Reset])));
				AddDependency(EPipelineStateObject::Particles_Reset, { CS_ParticleReset }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Simulate].Get();
				compute_pso_desc.CS = GetShader(CS_ParticleSimulate);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Simulate])));
				AddDependency(EPipelineStateObject::Particles_Simulate, { CS_ParticleSimulate }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Emit].Get();
				compute_pso_desc.CS = GetShader(CS_ParticleEmit);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Emit])));
				AddDependency(EPipelineStateObject::Particles_Emit, { CS_ParticleEmit }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_InitSortDispatchArgs].Get();
				compute_pso_desc.CS = GetShader(CS_ParticleInitSortDispatchArgs);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_InitSortDispatchArgs])));
				AddDependency(EPipelineStateObject::Particles_InitSortDispatchArgs, { CS_ParticleInitSortDispatchArgs }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Particles_Sort].Get();
				compute_pso_desc.CS = GetShader(CS_ParticleBitonicSortStep);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_BitonicSortStep])));
				AddDependency(EPipelineStateObject::Particles_BitonicSortStep, { CS_ParticleBitonicSortStep }, compute_pso_desc);

				compute_pso_desc.CS = GetShader(CS_ParticleSort512);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_Sort512])));
				AddDependency(EPipelineStateObject::Particles_Sort512, { CS_ParticleSort512 }, compute_pso_desc);

				compute_pso_desc.CS = GetShader(CS_ParticleSortInner512);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Particles_SortInner512])));
				AddDependency(EPipelineStateObject::Particles_SortInner512, { CS_ParticleSortInner512 }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::ClusterBuilding].Get();
				compute_pso_desc.CS = GetShader(CS_ClusterBuilding);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusterBuilding])));
				AddDependency(EPipelineStateObject::ClusterBuilding, { CS_ClusterBuilding }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::ClusterCulling].Get();
				compute_pso_desc.CS = GetShader(CS_ClusterCulling);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusterCulling])));
				AddDependency(EPipelineStateObject::ClusterCulling, { CS_ClusterCulling }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::TiledLighting].Get();
				compute_pso_desc.CS = GetShader(CS_TiledLighting);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::TiledLighting])));
				AddDependency(EPipelineStateObject::TiledLighting, { CS_TiledLighting }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Blur].Get();
				compute_pso_desc.CS = GetShader(CS_Blur_Horizontal);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Blur_Horizontal])));
				AddDependency(EPipelineStateObject::Blur_Horizontal, { CS_Blur_Horizontal }, compute_pso_desc);

				compute_pso_desc.CS = GetShader(CS_Blur_Vertical);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Blur_Vertical])));
				AddDependency(EPipelineStateObject::Blur_Vertical, { CS_Blur_Vertical }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::BloomExtract].Get();
				compute_pso_desc.CS = GetShader(CS_BloomExtract);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BloomExtract])));
				AddDependency(EPipelineStateObject::BloomExtract, { CS_BloomExtract }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::BloomCombine].Get();
				compute_pso_desc.CS = GetShader(CS_BloomCombine);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BloomCombine])));
				AddDependency(EPipelineStateObject::BloomCombine, { CS_BloomCombine }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::GenerateMips].Get();
				compute_pso_desc.CS = GetShader(CS_GenerateMips);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GenerateMips])));
				AddDependency(EPipelineStateObject::GenerateMips, { CS_GenerateMips }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::BokehGenerate].Get();
				compute_pso_desc.CS = GetShader(CS_BokehGenerate);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BokehGenerate])));
				AddDependency(EPipelineStateObject::BokehGenerate, { CS_BokehGenerate }, compute_pso_desc);
				///////////////
				compute_pso_desc.pRootSignature = rs_map[ERootSignature::FFT].Get();
				compute_pso_desc.CS = GetShader(CS_FFT_Horizontal);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FFT_Horizontal])));
				AddDependency(EPipelineStateObject::FFT_Horizontal, { CS_FFT_Horizontal }, compute_pso_desc);

				compute_pso_desc.CS = GetShader(CS_FFT_Vertical);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FFT_Vertical])));
				AddDependency(EPipelineStateObject::FFT_Vertical, { CS_FFT_Vertical }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::InitialSpectrum].Get();
				compute_pso_desc.CS = GetShader(CS_InitialSpectrum);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::InitialSpectrum])));
				AddDependency(EPipelineStateObject::InitialSpectrum, { CS_InitialSpectrum }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::InitialSpectrum].Get();
				compute_pso_desc.CS = GetShader(CS_InitialSpectrum);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::InitialSpectrum])));
				AddDependency(EPipelineStateObject::InitialSpectrum, { CS_InitialSpectrum }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::OceanNormalMap].Get();
				compute_pso_desc.CS = GetShader(CS_OceanNormalMap);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanNormalMap])));
				AddDependency(EPipelineStateObject::OceanNormalMap, { CS_OceanNormalMap }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Phase].Get();
				compute_pso_desc.CS = GetShader(CS_Phase);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Phase])));
				AddDependency(EPipelineStateObject::Phase, { CS_Phase }, compute_pso_desc);

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Spectrum].Get();
				compute_pso_desc.CS = GetShader(CS_Spectrum);
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Spectrum])));
				AddDependency(EPipelineStateObject::Spectrum, { CS_Spectrum }, compute_pso_desc);
			}
		}
	}
#endif

}

