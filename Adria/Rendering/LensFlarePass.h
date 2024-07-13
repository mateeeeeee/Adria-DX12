#pragma once
#include "TextureHandle.h"
#include "RenderGraph/RenderGraphResourceId.h"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GraphicsPipelineState;
	class ComputePipelineState;
	struct Light;

	class LensFlarePass
	{
	public:
		LensFlarePass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph, Light const& light);
		void AddPass2(RenderGraph& rendergraph, Light const& light);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::vector<TextureHandle> lens_flare_textures;
		std::unique_ptr<GraphicsPipelineState> lens_flare_pso;
		std::unique_ptr<ComputePipelineState> lens_flare_pso2;

	private:
		void CreatePSOs();
	};

}