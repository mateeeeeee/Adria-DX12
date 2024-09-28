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
		TAAPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void OnResize(uint32 w, uint32 h) override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;
		virtual bool IsGUIVisible(PostProcessor const*) const override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> taa_pso;

	private:
		void CreatePSO();
	};

}