#pragma once
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class RenderGraph;

	class GBufferPass
	{
	public:
		GBufferPass(entt::registry& reg, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);

		void OnRainEvent(bool enabled)
		{
			use_rain_pso = enabled;
		}
	private:
		entt::registry& reg;
		uint32 width, height;
		bool use_rain_pso = false;
	};
}