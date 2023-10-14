#pragma once
#include <memory>
#include "Core/CoreTypes.h"
#include "Graphics/GfxDescriptor.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxTexture;
	class GfxBuffer;
	class GfxDevice;

	class AutomaticExposurePass
	{
	public:
		AutomaticExposurePass(uint32 w, uint32 h);
		void OnSceneInitialized(GfxDevice* gfx);
		void AddPasses(RenderGraph& rg, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
		std::unique_ptr<GfxTexture> previous_ev100;
		GfxDescriptor previous_ev100_uav;
		std::unique_ptr<GfxBuffer> histogram_copy;
		bool invalid_history = true;

		float min_luminance = 0.0f;
		float max_luminance = 10.0f;
		float adaption_speed = 1.5f;
		float exposure_compensation = 0.0f;
		float low_percentile = 0.1f;
		float high_percentile = 0.9f;
		bool show_histogram = false;
	};
}


