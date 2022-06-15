#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"


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
		GBufferPass(tecs::registry& reg, GPUProfiler& gpu_profiler, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, bool profile_pass);

		void OnResize(uint32 w, uint32 h);

	private:
		tecs::registry& reg;
		GPUProfiler& gpu_profiler;
		uint32 width, height;
	};
}