#pragma once
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class BlurPass
	{
	public:
		explicit BlurPass(GfxDevice* gfx);
		~BlurPass();

		void AddPass(RenderGraph& rendergraph, RGResourceName src_texture, RGResourceName blurred_texture, char const* pass_name = "");

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxComputePipelineState> blur_horizontal_pso;
		std::unique_ptr<GfxComputePipelineState> blur_vertical_pso;
	private:
		void CreatePSOs();
	};
}