#include "ChromaticAberrationPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{

	ChromaticAberrationPass::ChromaticAberrationPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName ChromaticAberrationPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct ChromaticAberrationPassData
		{
			RGTextureReadOnlyId  input;
			RGTextureReadWriteId output;
		};

		rg.AddPass<ChromaticAberrationPassData>("Chromatic Aberration Pass",
			[=](ChromaticAberrationPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc chromatic_aberration_desc{};
				chromatic_aberration_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				chromatic_aberration_desc.width = width;
				chromatic_aberration_desc.height = height;

				builder.DeclareTexture(RG_RES_NAME(ChromaticAberrationOutput), chromatic_aberration_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(ChromaticAberrationOutput));
				data.input = builder.ReadTexture(input);
			},
			[=](ChromaticAberrationPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.input));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadWriteTexture(data.output));

				struct ChromaticAberrationConstants
				{
					float   intensity;
					uint32  input_idx;
					uint32  output_idx;
				} constants =
				{
					.intensity  = intensity,
					.input_idx  = i + 0,
					.output_idx = i + 1
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::ChromaticAberration));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx("Chromatic Aberration", ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderFloat("Intensity", &intensity, 0.0f, 40.0f);
					ImGui::TreePop();
				}
			}
		);

		return RG_RES_NAME(ChromaticAberrationOutput);
	}

	void ChromaticAberrationPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

