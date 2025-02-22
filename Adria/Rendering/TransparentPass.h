#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class TransparentPass
	{
	public:
		TransparentPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);
		~TransparentPass();

		void AddPass(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h);

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> transparent_psos;

	private:
		void CreatePSOs();
	};
}