#pragma once
#include <optional>
#include "../Core/Definitions.h"
#include "../Graphics/Buffer.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../tecs/entity.h"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class GraphicsDevice;

	class ClusteredLightingPass
	{
		static constexpr uint32 CLUSTER_SIZE_X = 16;
		static constexpr uint32 CLUSTER_SIZE_Y = 16;
		static constexpr uint32 CLUSTER_SIZE_Z = 16;
		static constexpr uint32 CLUSTER_COUNT = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
		static constexpr uint32 CLUSTER_MAX_LIGHTS = 128;

	public:
		ClusteredLightingPass(tecs::registry& reg, GraphicsDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, bool recreate_clusters);

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}

	private:
		tecs::registry& reg;
		uint32 width, height;
		//maybe move to render graph?
		Buffer clusters;
		Buffer light_counter;
		Buffer light_list;
		Buffer light_grid;
	};

}