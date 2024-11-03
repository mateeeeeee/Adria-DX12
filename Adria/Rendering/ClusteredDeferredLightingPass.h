#pragma once
#include "Graphics/GfxBuffer.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/entity.hpp"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxComputePipelineState;

	class ClusteredDeferredLightingPass
	{
		static constexpr Uint32 CLUSTER_SIZE_X = 16;
		static constexpr Uint32 CLUSTER_SIZE_Y = 16;
		static constexpr Uint32 CLUSTER_SIZE_Z = 16;
		static constexpr Uint32 CLUSTER_COUNT = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
		static constexpr Uint32 CLUSTER_MAX_LIGHTS = 128;

	public:
		ClusteredDeferredLightingPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);

		void AddPass(RenderGraph& rendergraph, Bool recreate_clusters);

		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
		}

	private:
		entt::registry& reg; 
		GfxDevice* gfx;
		Uint32 width, height;
		GfxBuffer clusters;
		GfxBuffer light_counter;
		GfxBuffer light_list;
		GfxBuffer light_grid;

		std::unique_ptr<GfxComputePipelineState> clustered_lighting_pso;
		std::unique_ptr<GfxComputePipelineState> clustered_building_pso;
		std::unique_ptr<GfxComputePipelineState> clustered_culling_pso;

	private:
		void CreatePSOs();
	};

}