#include "FilmEffectsPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{

	FilmEffectsPass::FilmEffectsPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName FilmEffectsPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct FilmEffectsPassData
		{
			RGTextureReadOnlyId  input;
			RGTextureReadWriteId output;
		};

		rg.AddPass<FilmEffectsPassData>("Film Effects Pass",
			[=](FilmEffectsPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc film_effects_desc{};
				film_effects_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				film_effects_desc.width = width;
				film_effects_desc.height = height;

				builder.DeclareTexture(RG_RES_NAME(FilmEffectsOutput), film_effects_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(FilmEffectsOutput));
				data.input = builder.ReadTexture(input);
			},
			[=](FilmEffectsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.input));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadWriteTexture(data.output));

				struct FilmEffectsConstants
				{
					bool32  chromatic_aberration_enabled;
					float   chromatic_aberration_intensity;
					bool32  vignette_enabled;
					float   vignette_intensity;
					uint32  input_idx;
					uint32  output_idx;
				} constants =
				{
					.chromatic_aberration_enabled = chromatic_aberration_enabled,
					.chromatic_aberration_intensity = chromatic_aberration_intensity,
					.vignette_enabled = vignette_enabled,
					.vignette_intensity = vignette_intensity,
					.input_idx  = i + 0,
					.output_idx = i + 1
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::FilmEffects));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx("Film Effects", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Chromatic Aberration", &chromatic_aberration_enabled);
					ImGui::Checkbox("Vignette", &vignette_enabled);
					if (chromatic_aberration_enabled)
					{
						ImGui::SliderFloat("Chromatic Aberration Intensity", &chromatic_aberration_intensity, 0.0f, 40.0f);
					}
					if (vignette_enabled)
					{
						ImGui::SliderFloat("Vignette Intensity", &vignette_intensity, 0.0f, 2.0f);
					}
					ImGui::TreePop();
				}
			}
		);

		return RG_RES_NAME(FilmEffectsOutput);
	}

	void FilmEffectsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

