#pragma once
#include <optional>
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceId.h"
#include "../../tecs/entity.h"

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
		TiledLightingPass(tecs::registry& reg, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		tecs::registry& reg;
		uint32 width, height;
	};

}