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

	class TiledLightingPass
	{
	public:
		TiledLightingPass(entt::registry& reg, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		entt::registry& reg;
		uint32 width, height;

		bool visualize_tiled = false;
		int32 visualize_max_lights = 16;
	};

}