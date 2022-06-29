#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class GPUProfiler;


	class GBufferPass
	{
	public:
		GBufferPass(entt::registry& reg, GPUProfiler& gpu_profiler, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, bool profile_pass);

		void OnResize(uint32 w, uint32 h);

	private:
		entt::registry& reg;
		GPUProfiler& gpu_profiler;
		uint32 width, height;
	};
}