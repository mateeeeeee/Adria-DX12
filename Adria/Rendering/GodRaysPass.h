#pragma once
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class ComputePipelineState;
	class RenderGraph;
	struct Light;

	class GodRaysPass
	{
	public:
		GodRaysPass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, Light const& light);
		void OnResize(uint32 w, uint32 h);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<ComputePipelineState> god_rays_pso;

	private:
		void CreatePSO();
	};


}