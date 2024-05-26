#pragma once
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	enum GfxShaderID : uint8;
	class RenderGraph;
	class GfxDevice;
	class GfxStateObject;

	class RayTracedShadowsPass
	{
	public:
		RayTracedShadowsPass(GfxDevice* gfx, uint32 width, uint32 height);
		~RayTracedShadowsPass();
		void AddPass(RenderGraph& rendergraph, uint32 light_index);
		void OnResize(uint32 w, uint32 h);

		bool IsSupported() const;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> ray_traced_shadows_so;
		uint32 width, height;
		bool is_supported;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderID shader);
	};
}