#pragma once
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class FXAAPass : public PostEffect
	{
	public:
		FXAAPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> fxaa_pso;

	private:
		void CreatePSO();
	};
}


