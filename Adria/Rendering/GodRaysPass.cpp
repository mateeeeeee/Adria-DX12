#include "GodRaysPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "PostProcessor.h"
#include "Graphics/GfxDevice.h" 
#include "Graphics/GfxPipelineState.h" 
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"


using namespace DirectX;

namespace adria
{

	GodRaysPass::GodRaysPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), copy_to_texture_pass(gfx, w, h)
	{
		CreatePSO();
	}

	void GodRaysPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		RGResourceName final_resource = postprocessor->GetFinalResource();
		entt::registry& reg = postprocessor->GetRegistry();
		auto lights = reg.view<Light>();
		for (entt::entity light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (!light_data.active) continue;

			if (light_data.type == LightType::Directional)
			{
				if (light_data.god_rays)
				{
					AddGodRaysPass(rg, light_data);
					copy_to_texture_pass.AddPass(rg, final_resource, RG_NAME(GodRaysOutput), BlendMode::AdditiveBlend);
				}
				else
				{
					copy_to_texture_pass.AddPass(rg, final_resource, RG_NAME(SunOutput), BlendMode::AdditiveBlend);
				}
				break;
			}
		}

	}

	void GodRaysPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		copy_to_texture_pass.OnResize(w, h);
	}

	void GodRaysPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_GodRays;
		god_rays_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	void GodRaysPass::AddGodRaysPass(RenderGraph& rg, Light const& light)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct GodRaysPassData
		{
			RGTextureReadOnlyId sun;
			RGTextureReadWriteId output;
		};

		rg.AddPass<GodRaysPassData>("God Rays Pass",
			[=](GodRaysPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc god_rays_desc{};
				god_rays_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				god_rays_desc.width = width;
				god_rays_desc.height = height;

				builder.DeclareTexture(RG_NAME(GodRaysOutput), god_rays_desc);
				data.output = builder.WriteTexture(RG_NAME(GodRaysOutput));
				data.sun = builder.ReadTexture(RG_NAME(SunOutput));
			},
			[=](GodRaysPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				if (light.type != LightType::Directional)
				{
					ADRIA_LOG(WARNING, "Using God Rays on a Non-Directional Light Source");
					return;
				}

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.sun),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				Vector4 camera_position(frame_data.camera_position);
				Vector4 light_pos = Vector4::Transform(light.position, XMMatrixTranslation(camera_position.x, 0.0f, camera_position.y));
				light_pos = Vector4::Transform(light_pos, frame_data.camera_viewproj);

				struct GodRaysConstants
				{
					float  light_screen_space_position_x;
					float  light_screen_space_position_y;
					float  density;
					float  weight;
					float  decay;
					float  exposure;

					uint32   sun_idx;
					uint32   output_idx;
				} constants =
				{
					.light_screen_space_position_x = 0.5f * light_pos.x / light_pos.w + 0.5f,
					.light_screen_space_position_y = -0.5f * light_pos.y / light_pos.w + 0.5f,
					.density = light.godrays_density, .weight = light.godrays_weight,
					.decay = light.godrays_decay, .exposure = light.godrays_exposure,
					.sun_idx = i, .output_idx = i + 1
				};
				cmd_list->SetPipelineState(god_rays_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

}
