#pragma once
#include "PostEffect.h"
#include "Graphics/GfxPipelineStatePermutationsFwd.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class BloomPass : public PostEffect
	{
	public:
		BloomPass(GfxDevice* gfx, uint32 w, uint32 h);
		~BloomPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::unique_ptr<GfxComputePipelineStatePermutations> downsample_psos;
		std::unique_ptr<GfxComputePipelineState> upsample_pso;

	private:
		void CreatePSOs();

		RGResourceName DownsamplePass(RenderGraph& rendergraph, RGResourceName input, uint32 pass_idx);
		RGResourceName UpsamplePass(RenderGraph& rendergraph, RGResourceName input, RGResourceName, uint32 pass_idx);
	};

	
}