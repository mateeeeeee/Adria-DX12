#pragma once
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class MotionVectorsPass : public PostEffect
	{
	public:
		MotionVectorsPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph& rg, PostProcessor*) override;
		virtual void OnResize(uint32 w, uint32 h) override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> motion_vectors_pso;

	private:
		void CreatePSO();
	};
}