#pragma once
#include "../Core/CoreTypes.h"
#include "../RenderGraph/RenderGraphResourceId.h"
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

	private:
		entt::registry& reg;
		uint32 width, height;
	};
}