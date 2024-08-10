#include "FXAAPass.h"
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
	static TAutoConsoleVariable<bool> FXAA("r.FXAA", true, "Enable or Disable FXAA");

	FXAAPass::FXAAPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void FXAAPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct FXAAPassData
		{
			RGTextureReadWriteId	output;
			RGTextureReadOnlyId		ldr;
		};

		rg.AddPass<FXAAPassData>("FXAA Pass",
			[=](FXAAPassData& data, RenderGraphBuilder& builder)
			{
				data.ldr = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
				ADRIA_ASSERT(builder.IsTextureDeclared(RG_NAME(FinalTexture)));
				data.output = builder.WriteTexture(RG_NAME(FinalTexture));
			},
			[=](FXAAPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.ldr),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct FXAAConstants
				{
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};

				cmd_list->SetPipelineState(fxaa_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	bool FXAAPass::IsEnabled(PostProcessor const*) const
	{
		return FXAA.Get();
	}

	void FXAAPass::GUI()
	{
		QueueGUI([&]()
			{
				ImGui::Checkbox("FXAA", FXAA.GetPtr());
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Antialiasing);
	}

	void FXAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void FXAAPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Fxaa;
		fxaa_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}
