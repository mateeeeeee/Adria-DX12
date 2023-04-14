#pragma once
#include "Enums.h"
#include "BlurPass.h"
#include "Graphics/GfxRayTracingUtil.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;

	class RayTracedReflectionsPass
	{
	public:
		RayTracedReflectionsPass(GfxDevice* gfx, uint32 width, uint32 height);
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		bool IsSupported() const;

	private:
		GfxDevice* gfx;
		BlurPass blur_pass;
		ArcPtr<ID3D12StateObject> ray_traced_reflections;
		uint32 width, height;
		bool is_supported;
		float reflection_roughness_scale = 0.0f;
	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderID shader);
	};
}