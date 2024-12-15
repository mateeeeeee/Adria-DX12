#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"

namespace adria
{
	enum class RendererOutput : Uint32
	{
		Final,
		Diffuse,
		WorldNormal,
		Roughness,
		Metallic,
		Emissive,
		AmbientOcclusion,
		IndirectLighting,
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

	private:
		GfxDevice* gfx;
		Uint32 width;
		Uint32 height;
		std::unique_ptr<GfxComputePipelineStatePermutations> renderer_output_psos;

	private:
		void CreatePSOs();
	};
}