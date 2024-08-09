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
		FXAAPass(GfxDevice* gfx, uint32 w, uint32 h);
		
		virtual void OnResize(uint32 w, uint32 h) override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> fxaa_pso;

	private:
		void CreatePSO();
	};
}


