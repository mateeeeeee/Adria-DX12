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

		if (EnableTransparentReflections.Get())
		{
			//copy scene texture for reflection to be sampled
		}

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<void>("Transparent Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.ReadDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
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

				auto batch_view = reg.view<Batch, Transparent>();
				for (auto batch_entity : batch_view)
				{
					Batch& batch = batch_view.get<Batch>(batch_entity);
					if (!batch.camera_visibility) continue;

					GfxPipelineState* pso = transparent_psos->Get();
					cmd_list->SetPipelineState(pso);

					struct TransparentConstants
					{
						Uint32 instance_id;
					} constants{ .instance_id = batch.instance_id };
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

