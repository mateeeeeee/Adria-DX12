#include "SimpleDepthOfFieldPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "PostProcessor.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

using namespace DirectX;

namespace adria
{
	static TAutoConsoleVariable<bool> SimpleDepthOfField("r.SimpleDepthOfField", true, "0 - Disabled, 1 - Enabled");

	SimpleDepthOfFieldPass::SimpleDepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), bokeh_pass(gfx, w, h), blur_pass(gfx, w, h)
	{
		CreatePSO();
	}

	void SimpleDepthOfFieldPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		blur_pass.AddPass(rg, postprocessor->GetFinalResource(), RG_NAME(BlurredDofInput), "Depth of Field Blur");

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.GetBlackboard().Create<DoFBlackboardData>(params.focus_distance, params.focus_radius);

		struct DepthOfFieldPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId blurred_input;
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};
		bokeh_pass.AddPass(rg, postprocessor->GetFinalResource());

		rg.AddPass<DepthOfFieldPassData>("Depth Of Field Pass",
			[=](DepthOfFieldPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc dof_output_desc{};
				dof_output_desc.width = width;
				dof_output_desc.height = height;
				dof_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_NAME(DepthOfFieldOutput), dof_output_desc);
				data.output = builder.WriteTexture(RG_NAME(DepthOfFieldOutput));
				data.input = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_PixelShader);
				data.blurred_input = builder.ReadTexture(RG_NAME(BlurredDofInput), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_PixelShader);
			},
			[=](DepthOfFieldPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(dof_pso.get());

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadOnlyTexture(data.input),
					ctx.GetReadOnlyTexture(data.blurred_input),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct DoFConstants
				{
					Vector2 dof_params;
					uint32 depth_idx;
					uint32 scene_idx;
					uint32 blurred_idx;
					uint32 output_idx;
				} constants =
				{
					.dof_params = Vector2(params.focus_distance, params.focus_radius),
					.depth_idx = i, .scene_idx = i + 1, .blurred_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		postprocessor->SetFinalResource(RG_NAME(DepthOfFieldOutput));
	}

	void SimpleDepthOfFieldPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		bokeh_pass.OnResize(w, h);
		blur_pass.OnResize(w, h);
	}

	bool SimpleDepthOfFieldPass::IsEnabled(PostProcessor const*) const
	{
		return SimpleDepthOfField.Get();
	}

	void SimpleDepthOfFieldPass::OnSceneInitialized()
	{
		bokeh_pass.OnSceneInitialized();
	}

	void SimpleDepthOfFieldPass::GUI()
	{
		GUI_Command([&]()
			{
				if (ImGui::TreeNodeEx("Simple Depth Of Field", 0))
				{
					ImGui::Checkbox("Enable", SimpleDepthOfField.GetPtr());
					if (SimpleDepthOfField.Get())
					{
						ImGui::SliderFloat("Focus Distance", &params.focus_distance, 0.0f, 1000.0f);
						ImGui::SliderFloat("Focus Radius", &params.focus_radius, 0.0f, 1000.0f);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessor);

	}

	void SimpleDepthOfFieldPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Dof;
		dof_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

