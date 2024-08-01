#pragma once
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxTexture;
	class GfxBuffer;
	class GfxDevice;
	class GfxComputePipelineState;

	class AutomaticExposurePass
	{
	public:
		AutomaticExposurePass(GfxDevice* gfx, uint32 w, uint32 h);
		void OnSceneInitialized();
		void AddPasses(RenderGraph& rg, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

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


