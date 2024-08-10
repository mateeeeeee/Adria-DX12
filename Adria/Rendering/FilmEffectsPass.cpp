#include "FilmEffectsPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "PostProcessor.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<bool> FilmEffects("r.FilmEffects", false, "Enable or Disable Film Effects");

	FilmEffectsPass::FilmEffectsPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void FilmEffectsPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
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

				builder.DeclareTexture(RG_NAME(FilmEffectsOutput), film_effects_desc);
				data.output = builder.WriteTexture(RG_NAME(FilmEffectsOutput));
				data.input = builder.ReadTexture(postprocessor->GetFinalResource());
			},
			[=](FilmEffectsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.input),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct FilmEffectsConstants
				{
					bool32  lens_distortion_enabled;
					float	lens_distortion_intensity;
					bool32  chromatic_aberration_enabled;
					float   chromatic_aberration_intensity;
					bool32  vignette_enabled;
					float   vignette_intensity;
					bool32  film_grain_enabled;
					float   film_grain_scale;
					float   film_grain_amount;
					uint32  film_grain_seed;
					uint32  input_idx;
					uint32  output_idx;
				} constants =
				{
					.lens_distortion_enabled = lens_distortion_enabled,
					.lens_distortion_intensity = lens_distortion_intensity,
					.chromatic_aberration_enabled = chromatic_aberration_enabled,
					.chromatic_aberration_intensity = chromatic_aberration_intensity,
					.vignette_enabled = vignette_enabled,
					.vignette_intensity = vignette_intensity,
					.film_grain_enabled = film_grain_enabled,
					.film_grain_scale = film_grain_scale,
					.film_grain_amount = film_grain_amount,
					.film_grain_seed = GetFilmGrainSeed(frame_data.delta_time, film_grain_seed_update_rate),
					.input_idx = i + 0,
					.output_idx = i + 1
				};
				cmd_list->SetPipelineState(film_effects_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootCBV(2, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		postprocessor->SetFinalResource(RG_NAME(FilmEffectsOutput));
	}

	void FilmEffectsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	bool FilmEffectsPass::IsEnabled(PostProcessor const*) const
	{
		return FilmEffects.Get();
	}

	void FilmEffectsPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Film Effects", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Enable Film Effects", FilmEffects.GetPtr());
					if (FilmEffects.Get())
					{
						ImGui::Checkbox("Lens Distortion", &lens_distortion_enabled);
						ImGui::Checkbox("Chromatic Aberration", &chromatic_aberration_enabled);
						ImGui::Checkbox("Vignette", &vignette_enabled);
						ImGui::Checkbox("Film Grain", &film_grain_enabled);
						if (lens_distortion_enabled)
						{
							ImGui::SliderFloat("Lens Distortion Intensity", &lens_distortion_intensity, -1.0f, 1.0f);
						}
						if (chromatic_aberration_enabled)
						{
							ImGui::SliderFloat("Chromatic Aberration Intensity", &chromatic_aberration_intensity, 0.0f, 40.0f);
						}
						if (vignette_enabled)
						{
							ImGui::SliderFloat("Vignette Intensity", &vignette_intensity, 0.0f, 2.0f);
						}
						if (film_grain_enabled)
						{
							ImGui::SliderFloat("Film Grain Scale", &film_grain_scale, 0.01f, 20.0f);
							ImGui::SliderFloat("Film Grain Amount", &film_grain_amount, 0.0f, 20.0f);
							ImGui::SliderFloat("Film Grain Seed Update Rate", &film_grain_seed_update_rate, 0.0f, 0.1f);
						}
					}
					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessing
		);
	}

	void FilmEffectsPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_FilmEffects;
		film_effects_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	uint32 FilmEffectsPass::GetFilmGrainSeed(float dt, float seed_update_rate)
	{
		static uint32 seed_counter = 0;
		static float time_counter = 0.0;
		time_counter += dt;
		if (time_counter >= seed_update_rate)
		{
			++seed_counter;
			time_counter = 0.0;
		}
		return seed_counter;
	}

}

