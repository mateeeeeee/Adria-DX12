#include "PSOCache.h"
#include "ShaderCache.h"
#include "../Graphics/GfxDevice.h"
#include "../Graphics/GfxStates.h"
#include "../Graphics/GfxShaderCompiler.h"
#include "../Logging/Logger.h"
#include "../Utilities/Timer.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	namespace
	{
		GfxDevice* gfx;
		HashMap<GfxPipelineStateID, std::unique_ptr<GraphicsPipelineState>> gfx_pso_map;
		HashMap<GfxPipelineStateID, std::unique_ptr<ComputePipelineState>>  compute_pso_map;

		enum EPipelineStateType
		{
			Graphics,
			Compute
		};

		inline GfxShader const& GetShader(GfxShaderID shader)
		{
			return ShaderCache::GetShader(shader);
		}
		void CreateAllPSOs()
		{
			GraphicsPipelineStateDesc gfx_pso_desc{};
			{
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Sky), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Sky;
				gfx_pso_desc.PS = PS_Skybox;
				gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Skybox] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_UniformColorSky;
				gfx_pso_map[GfxPipelineStateID::UniformColorSky] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_HosekWilkieSky;
				gfx_pso_map[GfxPipelineStateID::HosekWilkieSky] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_GBuffer), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_GBuffer;
				gfx_pso_desc.PS = PS_GBuffer;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 3u;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R8G8B8A8_UNORM;
				gfx_pso_desc.rtv_formats[1] = GfxFormat::R8G8B8A8_UNORM;
				gfx_pso_desc.rtv_formats[2] = GfxFormat::R8G8B8A8_UNORM;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::GBuffer] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				gfx_pso_desc.PS = PS_GBuffer_Mask;
				gfx_pso_map[GfxPipelineStateID::GBuffer_Mask] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
				gfx_pso_map[GfxPipelineStateID::GBuffer_Mask_NoCull] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				gfx_pso_desc.PS = PS_GBuffer;
				gfx_pso_map[GfxPipelineStateID::GBuffer_NoCull] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Shadow), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Shadow;
				gfx_pso_desc.PS = PS_Shadow;
				gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::Front;
				gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Solid;
				gfx_pso_desc.rasterizer_state.depth_bias = 7500;
				gfx_pso_desc.rasterizer_state.depth_bias_clamp = 0.0f;
				gfx_pso_desc.rasterizer_state.slope_scaled_depth_bias = 1.0f;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::LessEqual;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Shadow] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Shadow_Transparent), gfx_pso_desc.input_layout);
				gfx_pso_desc.VS = VS_Shadow_Transparent;
				gfx_pso_desc.PS = PS_Shadow_Transparent;
				gfx_pso_map[GfxPipelineStateID::Shadow_Transparent] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Sun), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Sun;
				gfx_pso_desc.PS = PS_Texture;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Sun] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_LensFlare;
				gfx_pso_desc.GS = GS_LensFlare;
				gfx_pso_desc.PS = PS_LensFlare;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				
				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Point;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;

				gfx_pso_map[GfxPipelineStateID::LensFlare] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				
				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_FullscreenQuad;
				gfx_pso_desc.PS = PS_Copy;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;

				gfx_pso_map[GfxPipelineStateID::Copy] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				gfx_pso_map[GfxPipelineStateID::Copy_AlphaBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				gfx_pso_map[GfxPipelineStateID::Copy_AdditiveBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
				
				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_FullscreenQuad;
				gfx_pso_desc.PS = PS_Add;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Add] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				gfx_pso_map[GfxPipelineStateID::Add_AlphaBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				gfx_pso_map[GfxPipelineStateID::Add_AdditiveBlend] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};	
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Bokeh;
				gfx_pso_desc.GS = GS_Bokeh;
				gfx_pso_desc.PS = PS_Bokeh;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				
				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Point;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Bokeh] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Ocean), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Ocean;
				gfx_pso_desc.PS = PS_Ocean;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::LessEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Ocean] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Wireframe;
				gfx_pso_map[GfxPipelineStateID::Ocean_Wireframe] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_OceanLOD;
				gfx_pso_desc.DS = DS_OceanLOD;
				gfx_pso_desc.HS = HS_OceanLOD;
				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Patch;
				gfx_pso_map[GfxPipelineStateID::OceanLOD_Wireframe] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Solid;
				gfx_pso_map[GfxPipelineStateID::OceanLOD] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Decals), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Decals;
				gfx_pso_desc.PS = PS_Decals;
				gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
				gfx_pso_desc.depth_state.depth_enable = FALSE;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R8G8B8A8_UNORM;
				gfx_pso_map[GfxPipelineStateID::Decals] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc.PS = PS_Decals_ModifyNormals;
				gfx_pso_desc.num_render_targets = 2;
				gfx_pso_desc.rtv_formats[1] = GfxFormat::R8G8B8A8_UNORM;
				gfx_pso_map[GfxPipelineStateID::Decals_ModifyNormals] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Simple), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Simple;
				gfx_pso_desc.PS = PS_Texture;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Texture] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);

				gfx_pso_desc = {};
				GfxShaderCompiler::CreateInputLayout(GetShader(VS_Simple), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Simple;
				gfx_pso_desc.PS = PS_Solid;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.depth_state.depth_enable = false;
				gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Wireframe;
				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Line;
				gfx_pso_map[GfxPipelineStateID::Solid_Wireframe] = std::make_unique<GraphicsPipelineState>(gfx, gfx_pso_desc);
			}

			ComputePipelineStateDesc compute_pso_desc{};
			compute_pso_desc.root_signature = GfxRootSignatureID::Common;
			{
				compute_pso_desc.CS = CS_ClusteredDeferredLighting;
				compute_pso_map[GfxPipelineStateID::ClusteredDeferredLighting] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_ClusterBuilding;
				compute_pso_map[GfxPipelineStateID::ClusterBuilding] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_ClusterCulling;
				compute_pso_map[GfxPipelineStateID::ClusterCulling] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_TiledDeferredLighting;
				compute_pso_map[GfxPipelineStateID::TiledDeferredLighting] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Picking;
				compute_pso_map[GfxPipelineStateID::Picking] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Blur_Horizontal;
				compute_pso_map[GfxPipelineStateID::Blur_Horizontal] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Blur_Vertical;
				compute_pso_map[GfxPipelineStateID::Blur_Vertical] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_BloomDownsample;
				compute_pso_map[GfxPipelineStateID::BloomDownsample] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_BloomDownsampleFirstPass;
				compute_pso_map[GfxPipelineStateID::BloomDownsample_FirstPass] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_BloomUpsample;
				compute_pso_map[GfxPipelineStateID::BloomUpsample] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_GenerateMips;
				compute_pso_map[GfxPipelineStateID::GenerateMips] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_BokehGeneration;
				compute_pso_map[GfxPipelineStateID::BokehGenerate] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_FFT_Horizontal;
				compute_pso_map[GfxPipelineStateID::FFT_Horizontal] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);
				compute_pso_desc.CS = CS_FFT_Vertical;
				compute_pso_map[GfxPipelineStateID::FFT_Vertical] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_InitialSpectrum;
				compute_pso_map[GfxPipelineStateID::InitialSpectrum] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_OceanNormals;
				compute_pso_map[GfxPipelineStateID::OceanNormals] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Phase;
				compute_pso_map[GfxPipelineStateID::Phase] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Spectrum;
				compute_pso_map[GfxPipelineStateID::Spectrum] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_BuildHistogram;
				compute_pso_map[GfxPipelineStateID::BuildHistogram] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_HistogramReduction;
				compute_pso_map[GfxPipelineStateID::HistogramReduction] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Exposure;
				compute_pso_map[GfxPipelineStateID::Exposure] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Ssao;
				compute_pso_map[GfxPipelineStateID::SSAO] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Hbao;
				compute_pso_map[GfxPipelineStateID::HBAO] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Ssr;
				compute_pso_map[GfxPipelineStateID::SSR] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Fog;
				compute_pso_map[GfxPipelineStateID::Fog] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Tonemap;
				compute_pso_map[GfxPipelineStateID::ToneMap] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_MotionVectors;
				compute_pso_map[GfxPipelineStateID::MotionVectors] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_MotionBlur;
				compute_pso_map[GfxPipelineStateID::MotionBlur] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Dof;
				compute_pso_map[GfxPipelineStateID::DOF] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_GodRays;
				compute_pso_map[GfxPipelineStateID::GodRays] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Fxaa;
				compute_pso_map[GfxPipelineStateID::FXAA] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Ambient;
				compute_pso_map[GfxPipelineStateID::Ambient] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Clouds;
				compute_pso_map[GfxPipelineStateID::Clouds] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_Taa;
				compute_pso_map[GfxPipelineStateID::TAA] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_DeferredLighting;
				compute_pso_map[GfxPipelineStateID::DeferredLighting] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);

				compute_pso_desc.CS = CS_VolumetricLighting;
				compute_pso_map[GfxPipelineStateID::VolumetricLighting] = std::make_unique<ComputePipelineState>(gfx, compute_pso_desc);
			}
		}
	}

	void PSOCache::Initialize(GfxDevice* _gfx)
	{
		gfx = _gfx;
		CreateAllPSOs();
	}

	void PSOCache::Destroy()
	{
		gfx_pso_map.clear();
		compute_pso_map.clear();
	}

	ID3D12PipelineState* PSOCache::Get(GfxPipelineStateID ps)
	{
		if (compute_pso_map.contains(ps)) return *compute_pso_map[ps];
		else return *gfx_pso_map[ps];
	}
}

