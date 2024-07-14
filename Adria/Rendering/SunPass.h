#pragma once
#include "RenderGraph/RenderGraphResourceName.h"
#include "entt/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class GfxGraphicsPipelineState;
	class RenderGraph;

	class SunPass
	{
	public:
		SunPass(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

		void AddPass(RenderGraph&, RGResourceName final_resource, entt::entity sun);

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		uint32 width;
		uint32 height;
		std::unique_ptr<GfxGraphicsPipelineState> sun_pso;

	private:
		void CreatePSO();
	};
}