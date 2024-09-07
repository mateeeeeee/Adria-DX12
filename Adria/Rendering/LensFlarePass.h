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
		LensFlarePass(GfxDevice* gfx, uint32 w, uint32 h);
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32 w, uint32 h) override;
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;
		virtual bool IsGUIVisible(PostProcessor const* postprocessor) const;

	private:
		GfxDevice* gfx;
		uint32 width, height;
		std::vector<TextureHandle> lens_flare_textures;
		std::unique_ptr<GfxGraphicsPipelineState> lens_flare_pso;
		std::unique_ptr<GfxComputePipelineState> procedural_lens_flare_pso;

	private:
		void CreatePSOs();
		void AddTextureBasedLensFlare(RenderGraph&, PostProcessor*, Light const&);
		void AddProceduralLensFlarePass(RenderGraph&, PostProcessor*, Light const&);
	};

}