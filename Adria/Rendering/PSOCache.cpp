#include "PSOCache.h"
#include "ShaderCache.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Graphics/GraphicsStates.h"
#include "../Graphics/ShaderCompiler.h"
#include "../Logging/Logger.h"
#include "../Utilities/Timer.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	namespace
	{
		GraphicsDevice* gfx;
		HashMap<EPipelineState, std::unique_ptr<GraphicsPipelineState>> gfx_pso_map;
		HashMap<EPipelineState, std::unique_ptr<ComputePipelineState>>  compute_pso_map;

		enum EPipelineStateType
		{
			Graphics,
			Compute
		};

		inline Shader const& GetShader(EShaderId shader)
		{
			return ShaderCache::GetShader(shader);
		}
		void CreateAllPSOs()
		{
			GraphicsPipelineStateDesc gfx_pso_desc{};
			{
				gfx_pso_desc.root_signature = ERootSignature::Particles_Shading;
				gfx_pso_desc.VS = VS_Particles;
				gfx_pso_desc.PS = PS_Particles;
				gfx_pso_desc.rasterizer_state.cull_mode = ECullMode::None;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_desc.depth_state.depth_enable = false;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::Particles_Shading] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_Skybox), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::Skybox;
				gfx_pso_desc.VS = VS_Skybox;
				gfx_pso_desc.PS = PS_Skybox;
				gfx_pso_desc.rasterizer_state.cull_mode = ECullMode::None;
				gfx_pso_desc.depth_state.depth_enable = TRUE;
				gfx_pso_desc.depth_state.depth_write_mask = EDepthWriteMask::Zero;
				gfx_pso_desc.depth_state.depth_func = EComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = EFormat::D32_FLOAT;
				gfx_pso_map[EPipelineState::Skybox] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.root_signature = ERootSignature::Sky;
				gfx_pso_desc.PS = PS_UniformColorSky;
				gfx_pso_map[EPipelineState::UniformColorSky] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				
				gfx_pso_desc.root_signature = ERootSignature::Sky;
				gfx_pso_desc.PS = PS_HosekWilkieSky;
				gfx_pso_map[EPipelineState::HosekWilkieSky] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.root_signature = ERootSignature::TAA;
				gfx_pso_desc.VS = VS_ScreenQuad;
				gfx_pso_desc.PS = PS_Taa;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::TAA] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_GBufferPBR), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::GbufferPBR;
				gfx_pso_desc.VS = VS_GBufferPBR;
				gfx_pso_desc.PS = PS_GBufferPBR;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = EDepthWriteMask::All;
				gfx_pso_desc.depth_state.depth_func = EComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 3u;
				gfx_pso_desc.rtv_formats[0] = EFormat::R8G8B8A8_UNORM;
				gfx_pso_desc.rtv_formats[1] = EFormat::R8G8B8A8_UNORM;
				gfx_pso_desc.rtv_formats[2] = EFormat::R8G8B8A8_UNORM;
				gfx_pso_desc.dsv_format = EFormat::D32_FLOAT;
				gfx_pso_map[EPipelineState::GBufferPBR] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_GBufferPBR_Mask;
				gfx_pso_map[EPipelineState::GBufferPBR_Mask] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.rasterizer_state.cull_mode = ECullMode::None;
				gfx_pso_map[EPipelineState::GBufferPBR_Mask_NoCull] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_GBufferPBR;
				gfx_pso_map[EPipelineState::GBufferPBR_NoCull] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = ERootSignature::LightingPBR;
				gfx_pso_desc.VS = VS_ScreenQuad;
				gfx_pso_desc.PS = PS_LightingPBR;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::One;
				gfx_pso_desc.depth_state.depth_enable = false;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::LightingPBR] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_LightingPBR_RayTracedShadows;
				gfx_pso_map[EPipelineState::LightingPBR_RayTracedShadows] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.root_signature = ERootSignature::ClusteredLightingPBR;
				gfx_pso_desc.PS = PS_ClusteredLightingPBR;
				gfx_pso_map[EPipelineState::ClusteredLightingPBR] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_DepthMap), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::DepthMap;
				gfx_pso_desc.VS = VS_DepthMap;
				gfx_pso_desc.PS = PS_DepthMap;
				gfx_pso_desc.rasterizer_state.cull_mode = ECullMode::Front;
				gfx_pso_desc.rasterizer_state.fill_mode = EFillMode::Solid;
				gfx_pso_desc.rasterizer_state.depth_bias = 7500;
				gfx_pso_desc.rasterizer_state.depth_bias_clamp = 0.0f;
				gfx_pso_desc.rasterizer_state.slope_scaled_depth_bias = 1.0f;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = EDepthWriteMask::All;
				gfx_pso_desc.depth_state.depth_func = EComparisonFunc::LessEqual;
				gfx_pso_desc.dsv_format = EFormat::D32_FLOAT;
				gfx_pso_map[EPipelineState::DepthMap] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				ShaderCompiler::CreateInputLayout(GetShader(VS_DepthMap_Transparent), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::DepthMap_Transparent;
				gfx_pso_desc.VS = VS_DepthMap_Transparent;
				gfx_pso_desc.PS = PS_DepthMap_Transparent;
				gfx_pso_map[EPipelineState::DepthMap_Transparent] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = ERootSignature::Volumetric;
				gfx_pso_desc.VS = VS_ScreenQuad;
				gfx_pso_desc.PS = PS_Volumetric_Directional;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::One;
				gfx_pso_desc.depth_state.depth_enable = false;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::Volumetric_Directional] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_Volumetric_DirectionalCascades;
				gfx_pso_map[EPipelineState::Volumetric_DirectionalCascades] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_Volumetric_Spot;
				gfx_pso_map[EPipelineState::Volumetric_Spot] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_Volumetric_Point;
				gfx_pso_map[EPipelineState::Volumetric_Point] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_Sun), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::Forward;
				gfx_pso_desc.VS = VS_Sun;
				gfx_pso_desc.PS = PS_Texture;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = EDepthWriteMask::Zero;
				gfx_pso_desc.depth_state.depth_func = EComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = EFormat::D32_FLOAT;
				gfx_pso_map[EPipelineState::Sun] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = ERootSignature::LensFlare;
				gfx_pso_desc.VS = VS_LensFlare;
				gfx_pso_desc.GS = GS_LensFlare;
				gfx_pso_desc.PS = PS_LensFlare;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				
				gfx_pso_desc.topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;

				gfx_pso_map[EPipelineState::LensFlare] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				
				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = ERootSignature::Copy;
				gfx_pso_desc.VS = VS_ScreenQuad;
				gfx_pso_desc.PS = PS_Copy;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;

				gfx_pso_map[EPipelineState::Copy] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_map[EPipelineState::Copy_AlphaBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_map[EPipelineState::Copy_AdditiveBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				
				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = ERootSignature::Add;
				gfx_pso_desc.VS = VS_ScreenQuad;
				gfx_pso_desc.PS = PS_Add;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::Add] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_map[EPipelineState::Add_AlphaBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				gfx_pso_map[EPipelineState::Add_AdditiveBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};	
				gfx_pso_desc.root_signature = ERootSignature::Bokeh;
				gfx_pso_desc.VS = VS_Bokeh;
				gfx_pso_desc.GS = GS_Bokeh;
				gfx_pso_desc.PS = PS_Bokeh;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = EBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = EBlendOp::Add;
				
				gfx_pso_desc.topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::Bokeh] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				
				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = ERootSignature::Clouds;
				gfx_pso_desc.VS = VS_ScreenQuad;
				gfx_pso_desc.PS = PS_VolumetricClouds;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::Clouds] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_Ocean), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::Ocean;
				gfx_pso_desc.VS = VS_Ocean;
				gfx_pso_desc.PS = PS_Ocean;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = EDepthWriteMask::All;
				gfx_pso_desc.depth_state.depth_func = EComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = EFormat::D32_FLOAT;
				gfx_pso_map[EPipelineState::Ocean] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.rasterizer_state.fill_mode = EFillMode::Wireframe;
				gfx_pso_map[EPipelineState::Ocean_Wireframe] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.root_signature = ERootSignature::OceanLOD;
				gfx_pso_desc.VS = VS_OceanLOD;
				gfx_pso_desc.DS = DS_OceanLOD;
				gfx_pso_desc.HS = HS_OceanLOD;
				gfx_pso_desc.topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
				gfx_pso_map[EPipelineState::OceanLOD_Wireframe] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.rasterizer_state.fill_mode = EFillMode::Solid;
				gfx_pso_map[EPipelineState::OceanLOD] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_Decals), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::Decals;
				gfx_pso_desc.VS = VS_Decals;
				gfx_pso_desc.PS = PS_Decals;
				gfx_pso_desc.rasterizer_state.cull_mode = ECullMode::None;
				gfx_pso_desc.depth_state.depth_enable = FALSE;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R8G8B8A8_UNORM;
				gfx_pso_map[EPipelineState::Decals] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_Decals_ModifyNormals;
				gfx_pso_desc.num_render_targets = 2;
				gfx_pso_desc.rtv_formats[1] = EFormat::R8G8B8A8_UNORM;
				gfx_pso_map[EPipelineState::Decals_ModifyNormals] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_Solid), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::Forward;
				gfx_pso_desc.VS = VS_Solid;
				gfx_pso_desc.PS = PS_Solid;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::Solid] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.depth_state.depth_enable = false;
				gfx_pso_desc.rasterizer_state.fill_mode = EFillMode::Wireframe;
				gfx_pso_desc.topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
				gfx_pso_map[EPipelineState::Solid_Wireframe] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				ShaderCompiler::CreateInputLayout(GetShader(VS_Texture), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = ERootSignature::Forward;
				gfx_pso_desc.VS = VS_Texture;
				gfx_pso_desc.PS = PS_Texture;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = EFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[EPipelineState::Texture] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
			}

			ComputePipelineStateDesc compute_pso_desc{};
			{
				compute_pso_desc.root_signature = ERootSignature::Picker;
				compute_pso_desc.CS = CS_Picker;
				compute_pso_map[EPipelineState::Picker] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Particles_InitDeadList;
				compute_pso_desc.CS = CS_ParticleInitDeadList;
				compute_pso_map[EPipelineState::Particles_InitDeadList] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Particles_Reset;
				compute_pso_desc.CS = CS_ParticleReset;

				compute_pso_desc.root_signature = ERootSignature::Particles_Simulate;
				compute_pso_desc.CS = CS_ParticleSimulate;
				compute_pso_map[EPipelineState::Particles_Simulate] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Particles_Emit;
				compute_pso_desc.CS = CS_ParticleEmit;
				compute_pso_map[EPipelineState::Particles_Emit] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Particles_InitSortDispatchArgs;
				compute_pso_desc.CS = CS_ParticleInitSortDispatchArgs;
				compute_pso_map[EPipelineState::Particles_InitSortDispatchArgs] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Particles_Sort;
				compute_pso_desc.CS = CS_ParticleBitonicSortStep;
				compute_pso_map[EPipelineState::Particles_BitonicSortStep] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_ParticleSort512;
				compute_pso_map[EPipelineState::Particles_Sort512] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_ParticleSortInner512;
				compute_pso_map[EPipelineState::Particles_SortInner512] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::ClusterBuilding;
				compute_pso_desc.CS = CS_ClusterBuilding;
				compute_pso_map[EPipelineState::ClusterBuilding] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::ClusterCulling;
				compute_pso_desc.CS = CS_ClusterCulling;
				compute_pso_map[EPipelineState::ClusterCulling] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::TiledLighting;
				compute_pso_desc.CS = CS_TiledLighting;
				compute_pso_map[EPipelineState::TiledLighting] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Blur;
				compute_pso_desc.CS = CS_Blur_Horizontal;
				compute_pso_map[EPipelineState::Blur_Horizontal] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Blur_Vertical;
				compute_pso_map[EPipelineState::Blur_Vertical] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::BloomExtract;
				compute_pso_desc.CS = CS_BloomExtract;
				compute_pso_map[EPipelineState::BloomExtract] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::BloomCombine;
				compute_pso_desc.CS = CS_BloomCombine;
				compute_pso_map[EPipelineState::BloomCombine] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::GenerateMips;
				compute_pso_desc.CS = CS_GenerateMips;
				compute_pso_map[EPipelineState::GenerateMips] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::BokehGenerate;
				compute_pso_desc.CS = CS_BokehGenerate;
				compute_pso_map[EPipelineState::BokehGenerate] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::FFT;
				compute_pso_desc.CS = CS_FFT_Horizontal;
				compute_pso_map[EPipelineState::FFT_Horizontal] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_FFT_Vertical;
				compute_pso_map[EPipelineState::FFT_Vertical] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::InitialSpectrum;
				compute_pso_desc.CS = CS_InitialSpectrum;
				compute_pso_map[EPipelineState::InitialSpectrum] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::InitialSpectrum;
				compute_pso_desc.CS = CS_InitialSpectrum;
				compute_pso_map[EPipelineState::InitialSpectrum] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::OceanNormalMap;
				compute_pso_desc.CS = CS_OceanNormalMap;
				compute_pso_map[EPipelineState::OceanNormalMap] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Phase;
				compute_pso_desc.CS = CS_Phase;
				compute_pso_map[EPipelineState::Phase] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Spectrum;
				compute_pso_desc.CS = CS_Spectrum;
				compute_pso_map[EPipelineState::Spectrum] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_BuildHistogram;
				compute_pso_map[EPipelineState::BuildHistogram] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_HistogramReduction;
				compute_pso_map[EPipelineState::HistogramReduction] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Exposure;
				compute_pso_map[EPipelineState::Exposure] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Ssao;
				compute_pso_map[EPipelineState::SSAO] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Hbao;
				compute_pso_map[EPipelineState::HBAO] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Ssr;
				compute_pso_map[EPipelineState::SSR] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Fog;
				compute_pso_map[EPipelineState::Fog] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Tonemap;
				compute_pso_map[EPipelineState::ToneMap] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_MotionVectors;
				compute_pso_map[EPipelineState::MotionVectors] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_MotionBlur;
				compute_pso_map[EPipelineState::MotionBlur] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Dof;
				compute_pso_map[EPipelineState::DOF] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_GodRays;
				compute_pso_map[EPipelineState::GodRays] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Fxaa;
				compute_pso_map[EPipelineState::FXAA] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.root_signature = ERootSignature::Common;
				compute_pso_desc.CS = CS_Ambient;
				compute_pso_map[EPipelineState::Ambient] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);
			}
		}
	}

	void PSOCache::Initialize(GraphicsDevice* _gfx)
	{
		gfx = _gfx;
		CreateAllPSOs();
	}

	void PSOCache::Destroy()
	{
		gfx_pso_map.clear();
		compute_pso_map.clear();
	}

	ID3D12PipelineState* PSOCache::Get(EPipelineState ps)
	{
		if (compute_pso_map.contains(ps)) return *compute_pso_map[ps];
		else return *gfx_pso_map[ps];
	}
}

