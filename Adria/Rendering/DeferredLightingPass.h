#pragma once
#include <optional>
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/entity.hpp"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	struct Light;


	class DeferredLightingPass
	{
	public:
		DeferredLightingPass(uint32 w, uint32 h) : width(w), height(h) {}
		void AddPass(RenderGraph& rendergraph, Light const& light, size_t light_id);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		uint32 width, height;
	};

}