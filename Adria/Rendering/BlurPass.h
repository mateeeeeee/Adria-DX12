#pragma once
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class ComputePipelineState;
	class RenderGraph;

	class BlurPass
	{
	public:
		BlurPass(GfxDevice* gfx, uint32 w, uint32 h);
		~BlurPass();

		void AddPass(RenderGraph& rendergraph,
			RGResourceName src_texture, RGResourceName blurred_texture, char const* pass_name = "");

		void OnResize(uint32 w, uint32 h);
		void SetResolution(uint32 w, uint32 h);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<ComputePipelineState> blur_horizontal_pso;
		std::unique_ptr<ComputePipelineState> blur_vertical_pso;
	private:
		void CreatePSOs();
	};
}