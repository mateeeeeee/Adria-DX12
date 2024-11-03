#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class GBufferPass
	{
	public:
		GBufferPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);
		~GBufferPass();

		void AddPass(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h);

		void OnRainEvent(Bool enabled)
		{
			use_rain_pso = enabled;
		}

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		Uint32 width, height;
		Bool use_rain_pso = false;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> gbuffer_psos;

	private:
		void CreatePSOs();
	};
}