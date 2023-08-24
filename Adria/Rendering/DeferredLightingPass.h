#pragma once
#include <unordered_set>
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "RenderGraph/RenderGraphResourceName.h"
#include "entt/entity/entity.hpp"

namespace adria
{
	class RenderGraph;

	class DeferredLightingPass
	{
	public:
		DeferredLightingPass(uint32 w, uint32 h) : width(w), height(h) {}
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
		uint32 width, height;
		std::vector<RGResourceName> shadow_textures;
	};

}