#pragma once
#include "TextureHandle.h"
#include "PostEffect.h"


namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;
	struct Light;

	class LensFlarePass : public PostEffect
	{
	public:
		LensFlarePass(GfxDevice* gfx, Uint32 w, Uint32 h);
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;
		virtual Bool IsGUIVisible(PostProcessor const* postprocessor) const;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::vector<TextureHandle> lens_flare_textures;
		std::unique_ptr<GfxGraphicsPipelineState> lens_flare_pso;
		std::unique_ptr<GfxComputePipelineState> procedural_lens_flare_pso;

	private:
		void CreatePSOs();
		void AddTextureBasedLensFlare(RenderGraph&, PostProcessor*, Light const&);
		void AddProceduralLensFlarePass(RenderGraph&, PostProcessor*, Light const&);
	};

}