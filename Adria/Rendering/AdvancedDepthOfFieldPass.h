#pragma once
#include "PostEffect.h"
#include "BlurPass.h"

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
		BlurPass blur_pass;

		std::unique_ptr<GfxComputePipelineState> compute_coc_pso;
		std::unique_ptr<GfxComputePipelineState> downsample_coc_pso;
		std::unique_ptr<GfxComputePipelineState> compute_prefiltered_texture_pso;

	private:
		void CreatePSOs();

		void AddComputeCircleOfConfusionPass(RenderGraph&);
		void AddDownsampleCircleOfConfusionPass(RenderGraph&);
		void AddComputePrefilteredTexturePass(RenderGraph&, RGResourceName color_texture);
	};

}