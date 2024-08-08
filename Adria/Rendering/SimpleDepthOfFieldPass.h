#pragma once
#include "BokehPass.h"
#include "BlurPass.h"
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class SimpleDepthOfFieldPass : public PostEffect
	{
		struct DoFParameters
		{
			float focus_distance = 200.0f;
			float focus_radius   = 25.0f;
		};
	public:
		SimpleDepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor*) const override;
		virtual void OnSceneInitialized() override;
		virtual void GUI();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		DoFParameters params{};

		BokehPass bokeh_pass;
		BlurPass blur_pass;

		std::unique_ptr<GfxComputePipelineState> dof_pso;

	private:
		void CreatePSO();
	};

}