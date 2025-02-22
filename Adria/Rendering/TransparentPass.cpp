#include "TransparentPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "RendererOutputPass.h"
#include "Graphics/GfxReflection.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"
#include "entt/entity/entity.hpp"


namespace adria
{
	static TAutoConsoleVariable<Bool> EnableTransparent("r.Transparent.Enable", false, "Enables or disables transparent pass");
	static TAutoConsoleVariable<Bool> EnableTransparentReflections("r.Transparent.Reflections", false, "Enables or disables reflections for transparent objects");

	TransparentPass::TransparentPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h)
		: reg { reg }, gfx{ gfx }, width{ w }, height{ h }
	{
		CreatePSOs();
		EnableTransparent->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) { transparent_changed.Broadcast(EnableTransparent.Get()); }));
	}

	TransparentPass::~TransparentPass() {}

	void TransparentPass::AddPass(RenderGraph& rg)
	{
		if (!EnableTransparent.Get()) return;
		RG_SCOPE(rg, "Transparent");

		Bool const reflections_enabled = EnableTransparentReflections.Get();
		if (reflections_enabled)
		{
			struct SceneTextureCopyPassData
			{
				RGTextureCopySrcId src;
				RGTextureCopyDstId dst;
			};

			rg.AddPass<SceneTextureCopyPassData>("Transparent Scene Texture Copy Pass",
				[=](SceneTextureCopyPassData& data, RenderGraphBuilder& builder)
				{
					RGTextureDesc tex_desc = builder.GetTextureDesc(RG_NAME(HDR_RenderTarget));
					builder.DeclareTexture(RG_NAME(HDR_RenderTarget_Copy), tex_desc);
					data.src = builder.ReadCopySrcTexture(RG_NAME(HDR_RenderTarget));
					data.dst = builder.WriteCopyDstTexture(RG_NAME(HDR_RenderTarget_Copy));
				},
				[=](SceneTextureCopyPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
				{
					GfxTexture const& src_texture = context.GetCopySrcTexture(data.src);
					GfxTexture& dst_texture = context.GetCopyDstTexture(data.dst);
					cmd_list->CopyTexture(dst_texture, src_texture);
				}, RGPassType::Copy, RGPassFlags::ForceNoCull);
		}

		struct TransparentPassData
		{
			RGTextureReadOnlyId scene;
			RGTextureReadOnlyId depth;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<TransparentPassData>("Transparent Pass",
			[=](TransparentPassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.ReadDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				if (reflections_enabled)
				{
					data.scene = builder.ReadTexture(RG_NAME(HDR_RenderTarget_Copy));
					data.depth = builder.ReadTexture(RG_NAME(DepthStencil));
				}
				builder.SetViewport(width, height);
			},
			[=](TransparentPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);

				Vector3 camera_position(frame_data.camera_position);
				reg.sort<Batch>([&camera_position](Batch const& lhs, Batch const& rhs)
					{
						Float lhs_distance = Vector3::DistanceSquared(camera_position, lhs.bounding_box.Center);
						Float rhs_distance = Vector3::DistanceSquared(camera_position, rhs.bounding_box.Center);
						return lhs_distance > rhs_distance;
					});

				transparent_psos->AddDefine("USE_SSR", reflections_enabled ? "1" : "0");
				GfxPipelineState* pso = transparent_psos->Get();
				cmd_list->SetPipelineState(pso);

				auto batch_view = reg.view<Batch, Transparent>();
				for (auto batch_entity : batch_view)
				{
					Batch& batch = batch_view.get<Batch>(batch_entity);
					if (!batch.camera_visibility) continue;

					struct TransparentConstants
					{
						Uint32 instance_id;
						Uint32 scene_idx;
						Uint32 depth_idx;
					} constants{ .instance_id = batch.instance_id, .scene_idx = 0, .depth_idx = 0 };
					if (reflections_enabled)
					{
						GfxDescriptor src_descriptors[] =
						{
							context.GetReadOnlyTexture(data.scene),
							context.GetReadOnlyTexture(data.depth)
						};
						GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
						gfx->CopyDescriptors(dst_descriptor, src_descriptors);
						Uint32 const i = dst_descriptor.GetIndex();
						constants.scene_idx = i;
						constants.depth_idx = i + 1;
					}
					cmd_list->SetRootConstants(1, constants);

					GfxIndexBufferView ibv(batch.submesh->buffer_address + batch.submesh->indices_offset, batch.submesh->indices_count);
					cmd_list->SetTopology(batch.submesh->topology);
					cmd_list->SetIndexBuffer(&ibv);
					cmd_list->DrawIndexed(batch.submesh->indices_count);
				}
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void TransparentPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	void TransparentPass::GUI()
	{
		QueueGUI([&]() 
		{
		if (ImGui::TreeNode("Transparent Settings"))
		{
			if (ImGui::Checkbox("Enable Transparent Pass", EnableTransparent.GetPtr()))
			{
				transparent_changed.Broadcast(EnableTransparent.Get());
			}
			if (EnableTransparent.Get())
			{
				ImGui::Checkbox("Enable Reflections", EnableTransparentReflections.GetPtr());
			}
			ImGui::TreePop();
		}}, GUICommandGroup_Renderer);
	}

	void TransparentPass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc transparent_pso_desc{};
		GfxReflection::FillInputLayoutDesc(GetGfxShader(VS_Transparent), transparent_pso_desc.input_layout);
		transparent_pso_desc.root_signature = GfxRootSignatureID::Common;
		transparent_pso_desc.VS = VS_Transparent;
		transparent_pso_desc.PS = PS_Transparent;
		transparent_pso_desc.depth_state.depth_enable = true;
		transparent_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
		transparent_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		transparent_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
		transparent_pso_desc.num_render_targets = 1u;
		transparent_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
		transparent_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
		transparent_pso_desc.blend_state.alpha_to_coverage_enable = false;
		transparent_pso_desc.blend_state.independent_blend_enable = false;
		auto& rt_blend = transparent_pso_desc.blend_state.render_target[0];
		rt_blend.blend_enable = true;
		rt_blend.src_blend = GfxBlend::SrcAlpha;
		rt_blend.dest_blend = GfxBlend::InvSrcAlpha;
		rt_blend.blend_op = GfxBlendOp::Add;
		rt_blend.src_blend_alpha = GfxBlend::One;
		rt_blend.dest_blend_alpha = GfxBlend::Zero;
		rt_blend.blend_op_alpha = GfxBlendOp::Add;
		rt_blend.render_target_write_mask = GfxColorWrite::EnableAll;
		transparent_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(gfx, transparent_pso_desc);
	}

}

