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
			float ssr_ray_step = 1.60f;
			float ssr_ray_hit_threshold = 2.00f;
		};
	public:
		SSRPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual void OnSceneInitialized() {}
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		SSRParameters params{};
		std::unique_ptr<GfxComputePipelineState> ssr_pso;
		
	private:
		void CreatePSO();
	};

}