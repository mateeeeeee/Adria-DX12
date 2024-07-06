#pragma once
#include "RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class ExponentialHeightFogPass
	{
		struct FogParameters
		{
			float  fog_falloff		= 0.125f;
			float  fog_density		= 3.0f;
			float  fog_height		= 100.0f;
			float  fog_min_opacity  = 0.7f;
			float  fog_start		= 0.0f;
			float  fog_cutoff_distance = 0.0f;

			float DirectionalInscatteringExponent = 4.0f;
			float  fog_color[3]		= { 0.5f,0.6f, 1.0f };
		};

	public:
		ExponentialHeightFogPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
		FogParameters params;
	};

}