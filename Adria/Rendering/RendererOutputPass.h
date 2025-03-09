#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"

namespace adria
{
	enum class RendererOutput : Uint32
	{
		Final,
		Diffuse,
		WorldNormal,
		Depth,
		Roughness,
		Metallic,
		Emissive,
		MaterialID,
		MeshletID,
		AmbientOcclusion,
		IndirectLighting,
		Custom,
		ShadingExtension,
		ViewMipMaps,
		TriangleOverdraw,
		MotionVectors,
		Count
	};

	class GfxDevice;
	class RenderGraph;

	class RendererOutputPass
	{
	public:
		RendererOutputPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~RendererOutputPass();

		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
		}
		void AddPass(RenderGraph&, RendererOutput);
		void GUI();

	private:
		GfxDevice* gfx;
		Uint32 width;
		Uint32 height;
		std::unique_ptr<GfxComputePipelineStatePermutations> renderer_output_psos;
		Float triangle_overdraw_scale = 1.0f;

	private:
		void CreatePSOs();
	};
}