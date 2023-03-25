#pragma once
#include <optional>
#include "../Core/CoreTypes.h"
#include "../RenderGraph/RenderGraphResourceId.h"
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

	private:
		uint32 width, height;
	};

}