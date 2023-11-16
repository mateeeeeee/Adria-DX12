#pragma once
#include "RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class ChromaticAberrationPass
	{
	public:
		ChromaticAberrationPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
		float intensity = 1.0f;
	};
}