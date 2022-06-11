#pragma once
#include <optional>
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../tecs/entity.h"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	struct Light;


	class LightingPass
	{
	public:
		LightingPass(uint32 w, uint32 h) : width(w), height(h) {}
		void AddPass(RenderGraph& rendergraph, Light const& light, size_t light_id);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		uint32 width, height;
	};

}