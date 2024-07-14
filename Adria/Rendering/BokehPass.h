#pragma once
#include "TextureHandle.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxBuffer;
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;

	class BokehPass
	{

		enum class BokehType : uint8
		{
			Hex,
			Oct,
			Circle,
			Cross,
			Count
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
		BokehPass(GfxDevice* gfx, uint32 w, uint32 h);
		~BokehPass();

		void AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		BokehParameters params{};
		TextureHandle bokeh_textures[(uint32)BokehType::Count] = { INVALID_TEXTURE_HANDLE };
		std::unique_ptr<GfxBuffer> counter_reset_buffer;
		std::unique_ptr<GfxBuffer> bokeh_indirect_buffer;

		std::unique_ptr<GfxGraphicsPipelineState> bokeh_draw_pso;
		std::unique_ptr<GfxComputePipelineState>  bokeh_generation_pso;

	private:
		void CreatePSOs();
		void AddGenerateBokehPass(RenderGraph& rg, RGResourceName input);
		void AddDrawBokehPass(RenderGraph& rg, RGResourceName input);
	};
}