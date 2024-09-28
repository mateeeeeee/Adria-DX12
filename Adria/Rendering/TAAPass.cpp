#include "TAAPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "PostProcessor.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"


namespace adria
{
	static TAutoConsoleVariable<bool> TAA("r.TAA", false, "Enable or Disable TAA");

	TAAPass::TAAPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void TAAPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct TAAPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId history;
			RGTextureReadOnlyId velocity;
			RGTextureReadWriteId output;
		};

		rg.AddPass<TAAPassData>("TAA Pass",
			[=](TAAPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc taa_desc{};
				taa_desc.width = width;
				taa_desc.height = height;
				taa_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_NAME(TAAOutput), taa_desc);
				data.output = builder.WriteTexture(RG_NAME(TAAOutput));
				data.input = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_PixelShader);
				data.history = builder.ReadTexture(RG_NAME(HistoryBuffer), ReadAccess_PixelShader);
				data.velocity = builder.ReadTexture(RG_NAME(VelocityBuffer), ReadAccess_PixelShader);
			},
			[=](TAAPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.input),
					ctx.GetReadOnlyTexture(data.history),
					ctx.GetReadOnlyTexture(data.velocity),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct TAAConstants
				{
					uint32 scene_idx;
					uint32 prev_scene_idx;
					uint32 velocity_idx;
					uint32 output_idx;
				} constants =
				{
					.scene_idx = i, .prev_scene_idx = i + 1, .velocity_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetPipelineState(taa_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		postprocessor->SetFinalResource(RG_NAME(TAAOutput));
	}

	bool TAAPass::IsEnabled(PostProcessor const* postprocessor) const
	{
		return TAA.Get() && !postprocessor->HasUpscaler();
	}

	void TAAPass::GUI()
	{
		QueueGUI([&]()
			{
				ImGui::Checkbox("TAA", TAA.GetPtr());
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Antialiasing);
	}

	bool TAAPass::IsGUIVisible(PostProcessor const* postprocessor) const
	{
		return !postprocessor->HasUpscaler();
	}

	void TAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void TAAPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Taa;
		taa_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}
}