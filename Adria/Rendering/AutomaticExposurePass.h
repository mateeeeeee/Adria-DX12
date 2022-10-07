#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class Texture;
	class GraphicsDevice;

	struct AutomaticExposureParameters
	{
		float32 min_luminance;
		float32 max_luminance;
		float32 adaption_speed;
		float32 exposure_compensation;
		float32 low_percentile;
		float32 high_percentile;
	};

	class AutomaticExposurePass
	{
	public:
		AutomaticExposurePass(uint32 w, uint32 h);
		void CreateResources(GraphicsDevice* gfx);
		void AddPasses(RenderGraph& rg, RGResourceName input, AutomaticExposureParameters const& params);
		void OnResize(uint32 w, uint32 h);
	private:
		uint32 width, height;
		std::unique_ptr<Texture> previous_ev100;
		bool invalid_history = true;
	};
}


