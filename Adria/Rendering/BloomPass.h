#pragma once
#include "Enums.h"
#include "GenerateMipsPass.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class BloomPass
	{
		struct BloomParameters
		{
			float bloom_threshold = 0.25f;
			float bloom_scale = 2.0f;
		};
	public:
		BloomPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
		GenerateMipsPass generate_mips_pass;
		BloomParameters params{};
	};

	
}