#pragma once
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxStateObject;
	class GfxShaderKey;

	class RayTracedShadowsPass
	{
	public:
		RayTracedShadowsPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~RayTracedShadowsPass();
		void AddPass(RenderGraph& rendergraph, Uint32 light_index);
		void OnResize(Uint32 w, Uint32 h);

		Bool IsSupported() const;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> ray_traced_shadows_so;
		Uint32 width, height;
		Bool is_supported;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}