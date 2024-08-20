#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"

namespace adria
{
	enum class RendererOutput : uint32
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
		static constexpr uint32 PERMUTATION_COUNT = (uint32)RendererOutput::Count;
	public:
		RendererOutputPass(GfxDevice* gfx, uint32 width, uint32 height);
		~RendererOutputPass();

		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}
		void AddPass(RenderGraph&, RendererOutput);

	private:
		GfxDevice* gfx;
		uint32 width;
		uint32 height;
		std::unique_ptr<GfxComputePipelineStatePermutations> renderer_output_psos;

	private:
		void CreatePSOs();
	};
}