#pragma once
#include "TextureHandle.h"
#include "PostEffect.h"

namespace adria
{
	class GfxDevice;
	class GfxComputePipelineState;
	class RenderGraph;

	class ToneMapPass : public PostEffect
	{
	public:
		ToneMapPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const { return true; }
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

		RGResourceName GetInput() const { return input; }

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		TextureHandle lens_dirt_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle tony_mc_mapface_lut_handle = INVALID_TEXTURE_HANDLE;
		std::unique_ptr<GfxComputePipelineState> tonemap_pso;
		RGResourceName input;

	private:
		void CreatePSO();
	};
}