#pragma once
#include "BlurPass.h"
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxStateObject;
	class GfxShaderKey;

	class RayTracedReflectionsPass
	{
	public:
		RayTracedReflectionsPass(GfxDevice* gfx, uint32 width, uint32 height);
		~RayTracedReflectionsPass();
		RGResourceName AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		bool IsSupported() const;

	private:
		GfxDevice* gfx;
		BlurPass blur_pass;
		std::unique_ptr<GfxStateObject> ray_traced_reflections_so;
		uint32 width, height;
		bool is_supported;
		float reflection_roughness_scale = 0.0f;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}