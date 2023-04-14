#pragma once
#include "Enums.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class BloomPass
	{
		struct BloomParameters
		{
			float radius = 0.75f;
			float bloom_intensity = 1.0f;
			float bloom_blend_factor = 0.25f;
		};
	public:
		BloomPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
		BloomParameters params{};

	private:

		RGResourceName DownsamplePass(RenderGraph& rendergraph, RGResourceName input, uint32 pass_idx);
		RGResourceName UpsamplePass(RenderGraph& rendergraph, RGResourceName input, RGResourceName, uint32 pass_idx);
	};

	
}