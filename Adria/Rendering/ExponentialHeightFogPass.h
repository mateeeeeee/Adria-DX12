#pragma once
#include "PostEffect.h"


namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class ExponentialHeightFogPass : public PostEffect
	{
		struct FogParameters
		{
			Float  fog_falloff		= 0.125f;
			Float  fog_density		= 3.0f;
			Float  fog_height		= 100.0f;
			Float  fog_min_opacity  = 0.7f;
			Float  fog_start		= 0.0f;
			Float  fog_cutoff_distance = 0.0f;
			Float  fog_color[3]		= { 0.5f, 0.6f, 1.0f };
		};

	public:
		ExponentialHeightFogPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> fog_pso;
		FogParameters params;

	private:
		void CreatePSO();
	};

}