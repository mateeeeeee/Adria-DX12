#pragma once
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class FilmEffectsPass : public PostEffect
	{
	public:
		FilmEffectsPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> film_effects_pso;

		Bool lens_distortion_enabled = false;
		Float lens_distortion_intensity = 0.2f;
		Bool chromatic_aberration_enabled = true;
		Float chromatic_aberration_intensity = 10.0f;
		Bool  vignette_enabled = true;
		Float vignette_intensity = 0.5f;
		Bool  film_grain_enabled = false;
		Float film_grain_scale = 3.0f;
		Float film_grain_amount = 0.5f;
		Float film_grain_seed_update_rate = 0.02f;

	private:
		void CreatePSO();
		static Uint32 GetFilmGrainSeed(Float dt, Float seed_update_rate);
	};
}