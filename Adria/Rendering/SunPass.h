#pragma once
#include "PostEffect.h"
#include "entt/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class GfxGraphicsPipelineState;
	class RenderGraph;

	class SunPass : public PostEffect
	{
	public:
		SunPass(GfxDevice* gfx, uint32 width, uint32 height);

		virtual bool IsEnabled(PostProcessor const*) const override
		{
			return true;
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;

		virtual void OnResize(uint32 w, uint32 h) override
		{
			width = w, height = h;
		}

	private:
		GfxDevice* gfx;
		uint32 width;
		uint32 height;
		std::unique_ptr<GfxGraphicsPipelineState> sun_pso;

	private:
		void CreatePSO();
	};
}