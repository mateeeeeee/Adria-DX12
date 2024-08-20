#pragma once
#include "PostEffect.h"
#include "BlurPass.h"
#include "Graphics/GfxPipelineStatePermutations.h"

namespace adria
{
	class GfxDevice;
	class GfxTexture;
	class GfxComputePipelineState;
	class RenderGraph;

	class DepthOfFieldPass : public PostEffect
	{
	public:
		DepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void OnSceneInitialized() override;
		virtual void GUI();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		BlurPass blur_pass;

		std::unique_ptr<GfxComputePipelineState> compute_coc_pso;
		std::unique_ptr<GfxComputePipelineState> downsample_coc_pso;
		std::unique_ptr<GfxComputePipelineState> compute_prefiltered_texture_pso;
		GfxComputePipelineStatePermutations<2> bokeh_first_pass_psos;

		std::unique_ptr<GfxTexture> bokeh_large_kernel;
		std::unique_ptr<GfxTexture> bokeh_small_kernel;

	private:
		void CreatePSOs();
		void CreateSmallBokehKernel();
		void CreateLargeBokehKernel();

		void AddComputeCircleOfConfusionPass(RenderGraph&);
		void AddDownsampleCircleOfConfusionPass(RenderGraph&);
		void AddComputePrefilteredTexturePass(RenderGraph&, RGResourceName);
		void AddBokehFirstPass(RenderGraph&, RGResourceName);
		void AddBokehSecondPass(RenderGraph&);
	};

}