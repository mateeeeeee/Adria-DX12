#pragma once
#include "BokehPass.h"
#include "BlurPass.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class ComputePipelineState;
	class RenderGraph;

	class DepthOfFieldPass
	{
		struct DoFParameters
		{
			float focus_distance = 200.0f;
			float focus_radius   = 25.0f;
		};
	public:
		DepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input, RGResourceName blurred_input);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		DoFParameters params{};

		BokehPass bokeh_pass;
		BlurPass blur_pass;

		std::unique_ptr<ComputePipelineState> dof_pso;

	private:
		void CreatePSO();
	};

}