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

		void SetAsyncCompute(Bool enable) { async_compute = enable; }
		void AddPass(RenderGraph& rendergraph, RGResourceName src_texture, RGResourceName blurred_texture, Char const* pass_name = "");

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxComputePipelineState> blur_horizontal_pso;
		std::unique_ptr<GfxComputePipelineState> blur_vertical_pso;
		Bool async_compute = false;

	private:
		void CreatePSOs();
	};
}