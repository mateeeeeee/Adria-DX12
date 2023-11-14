#pragma once
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxBuffer;

	class BokehPass
	{

		enum class BokehType : uint8
		{
			Hex,
			Oct,
			Circle,
			Cross
		};

		struct BokehParameters
		{
			float bokeh_blur_threshold = 0.9f;
			float bokeh_lum_threshold = 1.0f;
			float bokeh_radius_scale = 25.0f;
			float bokeh_color_scale = 1.0f;
			float bokeh_fallout = 0.9f;
			BokehType bokeh_type = BokehType::Hex;
		};

	public:
		BokehPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized(GfxDevice* gfx);

	private:
		uint32 width, height;
		BokehParameters params{};
		uint64 hex_bokeh_handle = -1;
		uint64 oct_bokeh_handle = -1;
		uint64 circle_bokeh_handle = -1;
		uint64 cross_bokeh_handle = -1;
		std::unique_ptr<GfxBuffer> counter_reset_buffer;
		std::unique_ptr<GfxBuffer> bokeh_indirect_buffer;
	private:
		void AddGenerateBokehPass(RenderGraph& rg, RGResourceName input);
		void AddDrawBokehPass(RenderGraph& rg, RGResourceName input);
	};
}