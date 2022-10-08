#pragma once
#include "Enums.h"
#include "GenerateMipsPass.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	struct BloomParameters
	{
		float32 bloom_threshold = 0.25f;
		float32 bloom_scale = 2.0f;
	};

	class BloomPass
	{
	public:
		BloomPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

		BloomParameters const& GetParams() const { return params; }
	private:
		uint32 width, height;
		GenerateMipsPass generate_mips_pass;
		BloomParameters params{};
	};

	
}