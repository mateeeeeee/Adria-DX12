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
			float  fog_falloff		= 0.125f;
			float  fog_density		= 3.0f;
			float  fog_height		= 100.0f;
			float  fog_min_opacity  = 0.7f;
			float  fog_start		= 0.0f;
			float  fog_cutoff_distance = 0.0f;
			float  fog_color[3]		= { 0.5f, 0.6f, 1.0f };
		};

	public:
		ExponentialHeightFogPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void OnResize(uint32 w, uint32 h) override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> fog_pso;
		FogParameters params;

	private:
		void CreatePSO();
	};

}