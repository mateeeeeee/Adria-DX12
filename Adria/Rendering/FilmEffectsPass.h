#pragma once
#include "RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class FilmEffectsPass
	{
	public:
		FilmEffectsPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
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
		static uint32 GetFilmGrainSeed(float dt, float seed_update_rate);
	};
}