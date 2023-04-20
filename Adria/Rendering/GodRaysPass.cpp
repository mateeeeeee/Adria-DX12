#include <DirectXMath.h>
#include "GodRaysPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"


using namespace DirectX;

namespace adria
{

	GodRaysPass::GodRaysPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void GodRaysPass::AddPass(RenderGraph& rg, Light const& light)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();

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

				builder.DeclareTexture(RG_RES_NAME(GodRaysOutput), god_rays_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(GodRaysOutput));
				data.sun = builder.ReadTexture(RG_RES_NAME(SunOutput));
			},
			[=](GodRaysPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				

				if (light.type != LightType::Directional)
				{
					ADRIA_LOG(WARNING, "Using God Rays on a Non-Directional Light Source");
					return;
				}

				uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.sun));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadWriteTexture(data.output));

				XMVECTOR camera_position = global_data.camera_position;
				XMVECTOR LightPos = XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position)));
				LightPos = XMVector4Transform(LightPos, global_data.camera_viewproj);
				XMFLOAT4 light_pos{};
				XMStoreFloat4(&light_pos, LightPos);
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
					.light_screen_space_position_x =  0.5f * light_pos.x / light_pos.w + 0.5f,
					.light_screen_space_position_y = -0.5f * light_pos.y / light_pos.w + 0.5f,
					.density = light.godrays_density, .weight = light.godrays_weight,
					.decay = light.godrays_decay, .exposure = light.godrays_exposure,
					.sun_idx = i, .output_idx = i + 1
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::GodRays));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void GodRaysPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}
