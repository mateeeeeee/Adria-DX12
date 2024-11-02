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
		BloomPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~BloomPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineStatePermutations> downsample_psos;
		std::unique_ptr<GfxComputePipelineState> upsample_pso;

	private:
		void CreatePSOs();

		RGResourceName DownsamplePass(RenderGraph& rendergraph, RGResourceName input, Uint32 pass_idx);
		RGResourceName UpsamplePass(RenderGraph& rendergraph, RGResourceName input, RGResourceName, Uint32 pass_idx);
	};

	
}