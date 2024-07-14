#pragma once
#include "RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class MotionVectorsPass
	{
	public:
		MotionVectorsPass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> motion_vectors_pso;

	private:
		void CreatePSO();
	};
}