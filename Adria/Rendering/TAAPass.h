#pragma once
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxTexture;
	class GfxComputePipelineState;
	class RenderGraph;

	class TAAPass : public PostEffect
	{
	public:
		TAAPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;
		virtual Bool IsGUIVisible(PostProcessor const*) const override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> taa_pso;

	private:
		void CreatePSO();
	};

}