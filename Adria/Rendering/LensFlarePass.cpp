#include "LensFlarePass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "PostProcessor.h" 
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "TextureManager.h"
#include "Core/Paths.h"
#include "Core/ConsoleManager.h"
#include "Logging/Logger.h"
#include "Editor/GUICommand.h"

using namespace DirectX;

namespace adria
{
	static TAutoConsoleVariable<int> LensFlare("r.LensFlare.Type", 0, "0 - procedural, 1 - texture-based");
	enum LensFlareType : uint8
	{
		LensFlareType_Procedural,
		LensFlareType_TextureBased,
	};

	LensFlarePass::LensFlarePass(GfxDevice* gfx, uint32 w, uint32 h)
		: gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}

	bool LensFlarePass::IsEnabled(PostProcessor const* postprocessor) const
	{
		auto lights = postprocessor->GetRegistry().view<Light>();
		for (entt::entity light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (light_data.active && light_data.lens_flare) return true;
		}
		return false;
	}

	void LensFlarePass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		auto lights = postprocessor->GetRegistry().view<Light>();
		for (entt::entity light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (!light_data.active || !light_data.lens_flare) continue;

			switch (LensFlare.Get())
			{
			case LensFlareType_Procedural:
				AddProceduralLensFlarePass(rg, postprocessor, light_data);
				break;
			case LensFlareType_TextureBased:
			default:
				AddTextureBasedLensFlare(rg, postprocessor, light_data);
				break;
			}
		}
	}

	void LensFlarePass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void LensFlarePass::OnSceneInitialized()
	{
		lens_flare_textures.push_back(g_TextureManager.LoadTexture(paths::TexturesDir + "LensFlare/flare0.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture(paths::TexturesDir + "LensFlare/flare1.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture(paths::TexturesDir + "LensFlare/flare2.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture(paths::TexturesDir + "LensFlare/flare3.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture(paths::TexturesDir + "LensFlare/flare4.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture(paths::TexturesDir + "LensFlare/flare5.jpg"));
		lens_flare_textures.push_back(g_TextureManager.LoadTexture(paths::TexturesDir + "LensFlare/flare6.jpg"));
	}

	void LensFlarePass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Lens Flare", ImGuiTreeNodeFlags_None))
				{
					ImGui::Combo("Lens Flare Type", LensFlare.GetPtr(), "Procedural\0Texture-based\0", 2);
					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessing
		);
	}

	bool LensFlarePass::IsGUIVisible(PostProcessor const* postprocessor) const
	{
		return IsEnabled(postprocessor);
	}

	void LensFlarePass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
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
		lens_flare_pso = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_LensFlare2;
		procedural_lens_flare_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	void LensFlarePass::AddTextureBasedLensFlare(RenderGraph& rg, PostProcessor* postprocessor, Light const& light)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct LensFlarePassData
		{
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<LensFlarePassData>("Lens Flare Pass",
			[=](LensFlarePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(postprocessor->GetFinalResource(), RGLoadStoreAccessOp::Preserve_Preserve);
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_PixelShader);
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
				Vector3 light_ss{};
				{
					Vector4 camera_position(frame_data.camera_position);
					Vector4 light_pos = light.type == LightType::Directional ? Vector4::Transform(light.position, XMMatrixTranslation(camera_position.x, 0.0f, camera_position.y)) : light.position;
					light_pos = Vector4::Transform(light_pos, frame_data.camera_viewproj);

					light_ss.x = 0.5f * light_pos.x / light_pos.w + 0.5f;
					light_ss.y = -0.5f * light_pos.y / light_pos.w + 0.5f;
					light_ss.z = light_pos.z / light_pos.w;
				}

				std::vector<GfxDescriptor> lens_flare_descriptors{};
				for (uint64 i = 0; i < lens_flare_textures.size(); ++i)
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
				cmd_list->SetPipelineState(lens_flare_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, constants2);
				cmd_list->SetTopology(GfxPrimitiveTopology::PointList);
				cmd_list->Draw(7);

			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void LensFlarePass::AddProceduralLensFlarePass(RenderGraph& rg, PostProcessor* postprocessor, Light const& light)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct LensFlarePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<LensFlarePassData>("Procedural Lens Flare Pass",
			[=](LensFlarePassData& data, RenderGraphBuilder& builder)
			{
				data.output = builder.WriteTexture(postprocessor->GetFinalResource());
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
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
					Vector4 camera_position(frame_data.camera_position);
					Vector4 light_pos = light.type == LightType::Directional ? Vector4::Transform(light.position, XMMatrixTranslation(camera_position.x, 0.0f, camera_position.y)) : light.position;
					light_pos = Vector4::Transform(light_pos, frame_data.camera_viewproj);
					light_ss.x = 0.5f * light_pos.x / light_pos.w + 0.5f;
					light_ss.y = -0.5f * light_pos.y / light_pos.w + 0.5f;
					light_ss.z = light_pos.z / light_pos.w;
				}
				GfxDescriptor src_descriptors[] =
				{
					context.GetReadOnlyTexture(data.depth),
					context.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

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

				cmd_list->SetPipelineState(procedural_lens_flare_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);

			}, RGPassType::Compute, RGPassFlags::None);
	}

}

