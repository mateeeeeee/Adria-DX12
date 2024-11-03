#pragma once
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class SSRPass : public PostEffect
	{
		struct SSRParameters
		{
			Float ssr_ray_step = 1.60f;
			Float ssr_ray_hit_threshold = 2.00f;
		};
	public:
		SSRPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual void OnSceneInitialized() {}
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		SSRParameters params{};
		std::unique_ptr<GfxComputePipelineState> ssr_pso;
		
	private:
		void CreatePSO();
	};

}