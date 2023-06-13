#pragma once
#include "Enums.h"
#include "Graphics/GfxRayTracingShaderTable.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;

	class RayTracedShadowsPass
	{
	public:
		RayTracedShadowsPass(GfxDevice* gfx, uint32 width, uint32 height);
		void AddPass(RenderGraph& rendergraph, uint32 light_index, RGResourceName mask_name);
		void OnResize(uint32 w, uint32 h);

		bool IsSupported() const;

	private:
		GfxDevice* gfx;
		ArcPtr<ID3D12StateObject> ray_traced_shadows;
		uint32 width, height;
		bool is_supported;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderID shader);
	};
}