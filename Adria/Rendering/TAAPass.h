#pragma once
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class TAAPass : public PostEffect
	{
	public:
		TAAPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const;
		virtual void GUI() override;
		virtual bool IsGUIVisible(PostProcessor const* postprocessor) const override;

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input, RGResourceName history);

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> taa_pso;

	private:
		void CreatePSO();
	};

}