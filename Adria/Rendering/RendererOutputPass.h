#pragma once
#include "Graphics/GfxPipelineStatePermutations.h"

namespace adria
{
	enum class RendererOutputType : uint32
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
		static constexpr uint32 PERMUTATION_COUNT = (uint32)RendererOutputType::Count;
	public:
		RendererOutputPass(GfxDevice* gfx, uint32 width, uint32 height);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}
		void AddPass(RenderGraph&, RendererOutputType);

	private:
		GfxDevice* gfx;
		GfxComputePipelineStatePermutations<PERMUTATION_COUNT> renderer_output_psos;
		uint32 width;
		uint32 height;

	private:
		void CreatePSOs();
	};
}