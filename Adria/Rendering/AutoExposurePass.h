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
		AutoExposurePass(GfxDevice* gfx, Uint32 w, Uint32 h);
		//void AddPasses(RenderGraph& rg, RGResourceName input);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxTexture> previous_ev100;
		GfxDescriptor previous_ev100_uav;
		std::unique_ptr<GfxBuffer> histogram_copy;
		Bool invalid_history = true;

		std::unique_ptr<GfxComputePipelineState> build_histogram_pso;
		std::unique_ptr<GfxComputePipelineState> histogram_reduction_pso;
		std::unique_ptr<GfxComputePipelineState> exposure_pso;

		Float min_luminance = 0.0f;
		Float max_luminance = 10.0f;
		Float adaption_speed = 1.5f;
		Float low_percentile = 0.49f;
		Float high_percentile = 0.99f;
		Bool show_histogram = false;

	private:
		void CreatePSOs();
	};
}


