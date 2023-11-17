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
		bool chromatic_aberration_enabled = true;
		float chromatic_aberration_intensity = 10.0f;
		bool  vignette_enabled = true;
		float vignette_intensity = 1.0f;
	};
}