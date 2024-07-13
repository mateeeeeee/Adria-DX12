#pragma once
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class ComputePipelineState;
	class RenderGraph;

	class TAAPass
	{
	public:
		TAAPass(GfxDevice* gfx, uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input, RGResourceName history);
		void OnResize(uint32 w, uint32 h);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<ComputePipelineState> taa_pso;

	private:
		void CreatePSO();
	};

}