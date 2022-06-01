#pragma once
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceRef.h"


namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class GPUProfiler;

	struct GBufferPassData
	{
		RGTextureRef gbuffer_normal;
		RGTextureRef gbuffer_albedo;
		RGTextureRef gbuffer_emissive;
		RGTextureRef depth_stencil;
		RGTextureDSVRef depth_stencil_dsv;
	};

	class GBufferPass
	{
	public:
		GBufferPass(tecs::registry& reg, GPUProfiler& gpu_profiler, uint32 w, uint32 h);

		GBufferPassData const& AddPass(RenderGraph& rendergraph, bool profile_pass);

		void OnResize(uint32 w, uint32 h);

	private:
		tecs::registry& reg;
		GPUProfiler& gpu_profiler;
		uint32 width, height;
	};

}