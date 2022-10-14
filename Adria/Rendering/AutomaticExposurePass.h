#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class Texture;
	class Buffer;
	class GraphicsDevice;

	class AutomaticExposurePass
	{
	public:
		AutomaticExposurePass(uint32 w, uint32 h);
		void OnSceneInitialized(GraphicsDevice* gfx);
		void AddPasses(RenderGraph& rg, RGResourceName input);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
		std::unique_ptr<Texture> previous_ev100;
		std::unique_ptr<Buffer> histogram_copy;
		bool invalid_history = true;

		float32 min_luminance = 0.0f;
		float32 max_luminance = 10.0f;
		float32 adaption_speed = 1.5f;
		float32 exposure_compensation = 0.75f;
		float32 low_percentile = 0.1f;
		float32 high_percentile = 0.9f;
		bool show_histogram = false;
	};
}


