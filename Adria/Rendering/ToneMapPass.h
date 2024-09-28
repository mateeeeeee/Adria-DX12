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
		ToneMapPass(GfxDevice* gfx, uint32 w, uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32, uint32) override;
		virtual bool IsEnabled(PostProcessor const*) const { return true; }
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

		RGResourceName GetInput() const { return input; }

	private:
		GfxDevice* gfx;
		uint32 width, height;
		TextureHandle lens_dirt_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle tony_mc_mapface_lut_handle = INVALID_TEXTURE_HANDLE;
		std::unique_ptr<GfxComputePipelineState> tonemap_pso;
		RGResourceName input;

	private:
		void CreatePSO();
	};
}