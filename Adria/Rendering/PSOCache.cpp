#include "PSOCache.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxStateObject.h"
#include "Graphics/GfxStates.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxReflection.h"
#include "Logging/Logger.h"
#include "Utilities/Timer.h"

namespace adria
{
	namespace
	{
		GfxDevice* gfx;
		std::unordered_map<GfxPipelineStateID, std::unique_ptr<GraphicsPipelineState>>		gfx_pso_map;
		std::unordered_map<GfxPipelineStateID, std::unique_ptr<ComputePipelineState>>		compute_pso_map;
		std::unordered_map<GfxPipelineStateID, std::unique_ptr<MeshShaderPipelineState>>    mesh_pso_map;

		inline GfxShader const& GetShader(ShaderID shader)
		{
			return ShaderManager::GetGfxShader(shader);
		}
		void CreateAllPSOs()
		{
			GraphicsPipelineStateDesc gfx_pso_desc{};
			{
				gfx_pso_desc = {};
				GfxReflection::FillInputLayoutDesc(GetShader(VS_Sun), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Sun;
				gfx_pso_desc.PS = PS_Texture;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Sun] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

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
				gfx_pso_map[GfxPipelineStateID::Bokeh] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);
			}

			ComputePipelineStateDesc compute_pso_desc{};
			compute_pso_desc.root_signature = GfxRootSignatureID::Common;
			{
				compute_pso_desc.CS = CS_Picking;
				compute_pso_map[GfxPipelineStateID::Picking] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Blur_Horizontal;
				compute_pso_map[GfxPipelineStateID::Blur_Horizontal] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Blur_Vertical;
				compute_pso_map[GfxPipelineStateID::Blur_Vertical] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_BokehGeneration;
				compute_pso_map[GfxPipelineStateID::BokehGenerate] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Ssao;
				compute_pso_map[GfxPipelineStateID::SSAO] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Hbao;
				compute_pso_map[GfxPipelineStateID::HBAO] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Ssr;
				compute_pso_map[GfxPipelineStateID::SSR] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_ExponentialHeightFog;
				compute_pso_map[GfxPipelineStateID::ExponentialHeightFog] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Tonemap;
				compute_pso_map[GfxPipelineStateID::ToneMap] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_MotionVectors;
				compute_pso_map[GfxPipelineStateID::MotionVectors] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_MotionBlur;
				compute_pso_map[GfxPipelineStateID::MotionBlur] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Dof;
				compute_pso_map[GfxPipelineStateID::DOF] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_GodRays;
				compute_pso_map[GfxPipelineStateID::GodRays] = gfx->CreateComputePipelineState(compute_pso_desc);
				
				compute_pso_desc.CS = CS_FilmEffects;
				compute_pso_map[GfxPipelineStateID::FilmEffects] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Fxaa;
				compute_pso_map[GfxPipelineStateID::FXAA] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Ambient;
				compute_pso_map[GfxPipelineStateID::Ambient] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Taa;
				compute_pso_map[GfxPipelineStateID::TAA] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_RTAOFilter;
				compute_pso_map[GfxPipelineStateID::RTAOFilter] = gfx->CreateComputePipelineState(compute_pso_desc);
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
		mesh_pso_map.clear();
	}

	GfxPipelineState* PSOCache::Get(GfxPipelineStateID ps)
	{
		if (compute_pso_map.contains(ps)) return compute_pso_map[ps].get();
		else if (mesh_pso_map.contains(ps)) return mesh_pso_map[ps].get();
		else return gfx_pso_map[ps].get();
	}
}

