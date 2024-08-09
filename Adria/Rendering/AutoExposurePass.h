#pragma once
#include "PostEffect.h"
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class RenderGraph;
	class GfxTexture;
	class GfxBuffer;
	class GfxDevice;
	class GfxComputePipelineState;

	class AutoExposurePass : public PostEffect
	{
	public:
		AutoExposurePass(GfxDevice* gfx, uint32 w, uint32 h);
		//void AddPasses(RenderGraph& rg, RGResourceName input);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxTexture> previous_ev100;
		GfxDescriptor previous_ev100_uav;
		std::unique_ptr<GfxBuffer> histogram_copy;
		bool invalid_history = true;

		std::unique_ptr<GfxComputePipelineState> build_histogram_pso;
		std::unique_ptr<GfxComputePipelineState> histogram_reduction_pso;
		std::unique_ptr<GfxComputePipelineState> exposure_pso;

		float min_luminance = 0.0f;
		float max_luminance = 10.0f;
		float adaption_speed = 1.5f;
		float low_percentile = 0.49f;
		float high_percentile = 0.99f;
		bool show_histogram = false;

	private:
		void CreatePSOs();
	};
}


