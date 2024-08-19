#pragma once
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class AdvancedDepthOfFieldPass : public PostEffect
	{
	public:
		AdvancedDepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> compute_coc_pso;

	private:
		void CreatePSOs();

		void AddComputeCircleOfConfusionPass(RenderGraph&);
	};

}