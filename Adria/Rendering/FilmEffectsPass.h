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
		FilmEffectsPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> film_effects_pso;

		bool lens_distortion_enabled = false;
		float lens_distortion_intensity = 0.2f;
		bool chromatic_aberration_enabled = true;
		float chromatic_aberration_intensity = 10.0f;
		bool  vignette_enabled = true;
		float vignette_intensity = 0.5f;
		bool  film_grain_enabled = false;
		float film_grain_scale = 3.0f;
		float film_grain_amount = 0.5f;
		float film_grain_seed_update_rate = 0.02f;

	private:
		void CreatePSO();
		static uint32 GetFilmGrainSeed(float dt, float seed_update_rate);
	};
}