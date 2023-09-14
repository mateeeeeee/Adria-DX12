#include "LensFlarePass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "RenderGraph/RenderGraph.h"
#include "TextureManager.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Logging/Logger.h"

using namespace DirectX;

namespace adria
{

	LensFlarePass::LensFlarePass(uint32 w, uint32 h)
		: width{ w }, height{ h }
	{}

	void LensFlarePass::AddPass(RenderGraph& rg, Light const& light)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct LensFlarePassData
		{
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<LensFlarePassData>("Lens Flare Pass",
			[=](LensFlarePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(PostprocessMain), RGLoadStoreAccessOp::Preserve_Preserve);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](LensFlarePassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				if (light.type != LightType::Directional)
				{
					ADRIA_LOG(WARNING, "Using Lens Flare on a Non-Directional Light Source");
					return;
				}
				XMFLOAT3 light_ss{};
				{
					auto camera_position = global_data.camera_position;
					XMVECTOR light_position = light.type == LightType::Directional ?
						XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position))) : XMVECTOR(light.position);
					XMVECTOR LightPos = XMVector4Transform(light_position, global_data.camera_viewproj);
					XMFLOAT4 light_pos{};
					XMStoreFloat4(&light_pos, LightPos);
					light_ss.x = 0.5f * light_pos.x / light_pos.w + 0.5f;
					light_ss.y = -0.5f * light_pos.y / light_pos.w + 0.5f;
					light_ss.z = light_pos.z / light_pos.w;
				}

				std::vector<GfxDescriptor> lens_flare_descriptors{};
				for (size_t i = 0; i < lens_flare_textures.size(); ++i)
					lens_flare_descriptors.push_back(g_TextureManager.GetSRV(lens_flare_textures[i]));
				lens_flare_descriptors.push_back(context.GetReadOnlyTexture(data.depth));

				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(8);
				gfx->CopyDescriptors(dst_descriptor, lens_flare_descriptors);
				uint32 i = dst_descriptor.GetIndex();

				struct LensFlareConstants
				{
					uint32   lens_idx0;
					uint32   lens_idx1;
					uint32   lens_idx2;
					uint32   lens_idx3;
					uint32   lens_idx4;
					uint32   lens_idx5;
					uint32   lens_idx6;
					uint32   depth_idx;
				} constants = 
				{
					.lens_idx0 = i, .lens_idx1 = i + 1, .lens_idx2 = i + 2, .lens_idx3 = i + 3,
					.lens_idx4 = i + 4, .lens_idx5 = i + 5, .lens_idx6 = i + 6, .depth_idx = i + 7
				};

				struct LensFlareConstants2
				{
					float light_ss_x;
					float light_ss_y;
					float light_ss_z;
				} constants2 = 
				{
					.light_ss_x = light_ss.x,
					.light_ss_y = light_ss.y,
					.light_ss_z = light_ss.z
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::LensFlare));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, constants2);
				cmd_list->SetTopology(GfxPrimitiveTopology::PointList);
				cmd_list->Draw(7);

			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void LensFlarePass::AddPass2(RenderGraph& rg, Light const& light)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct LensFlarePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<LensFlarePassData>("Lens Flare 2 Pass",
			[=](LensFlarePassData& data, RenderGraphBuilder& builder)
			{
				data.output = builder.WriteTexture(RG_RES_NAME(PostprocessMain));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](LensFlarePassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				if (light.type != LightType::Directional)
				{
					ADRIA_LOG(WARNING, "Using Lens Flare on a Non-Directional Light Source");
					return;
				}
				XMFLOAT3 light_ss{};
				{
					auto camera_position = global_data.camera_position;
					XMVECTOR light_position = light.type == LightType::Directional ?
						XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position))) : XMVECTOR(light.position);
					XMVECTOR LightPos = XMVector4Transform(light_position, global_data.camera_viewproj);
					XMFLOAT4 light_pos{};
					XMStoreFloat4(&light_pos, LightPos);
					light_ss.x = 0.5f * light_pos.x / light_pos.w + 0.5f;
					light_ss.y = -0.5f * light_pos.y / light_pos.w + 0.5f;
					light_ss.z = light_pos.z / light_pos.w;
				}
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(2);
				GfxDescriptor src_descriptors[] = { context.GetReadOnlyTexture(data.depth), context.GetReadWriteTexture(data.output) };
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 i = dst_descriptor.GetIndex();

				struct LensFlareConstants
				{
					float light_ss_x;
					float light_ss_y;
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.light_ss_x = light_ss.x,
					.light_ss_y = light_ss.y,
					.depth_idx = i + 0,
					.output_idx = i + 1
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::LensFlare2));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);

			}, RGPassType::Compute, RGPassFlags::None);
	}

	void LensFlarePass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void LensFlarePass::OnSceneInitialized(GfxDevice* gfx)
	{
		lens_flare_textures.push_back(g_TextureManager.LoadTexture("Resources/Textures/lensflare/flare0.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture("Resources/Textures/lensflare/flare1.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture("Resources/Textures/lensflare/flare2.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture("Resources/Textures/lensflare/flare3.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture("Resources/Textures/lensflare/flare4.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture("Resources/Textures/lensflare/flare5.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture("Resources/Textures/lensflare/flare6.jpg"));
	}

}

