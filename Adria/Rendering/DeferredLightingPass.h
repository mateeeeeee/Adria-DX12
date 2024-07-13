#pragma once
#include "RenderGraph/RenderGraphResourceId.h"
#include "RenderGraph/RenderGraphResourceName.h"
#include "entt/entity/entity.hpp"

namespace adria
{
	class GfxDevice;
	class ComputePipelineState;
	class RenderGraph;

	class DeferredLightingPass
	{
	public:
		DeferredLightingPass(GfxDevice* gfx, uint32 w, uint32 h);
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}
		
		void OnShadowTextureRendered(RGResourceName name)
		{
			shadow_textures.push_back(name);
		}

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::vector<RGResourceName> shadow_textures;
		std::unique_ptr<ComputePipelineState> deferred_lighting_pso;

	private:
		void CreatePSOs();
	};

}