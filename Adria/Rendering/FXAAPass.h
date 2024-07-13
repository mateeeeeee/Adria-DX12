#pragma once
#include "RenderGraph/RenderGraphResourceId.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class ComputePipelineState;
	class RenderGraph;

	class FXAAPass
	{
	public:
		FXAAPass(GfxDevice* gfx, uint32 w, uint32 h);
		void AddPass(RenderGraph& rg, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<ComputePipelineState> fxaa_pso;

	private:
		void CreatePSO();
	};
}


