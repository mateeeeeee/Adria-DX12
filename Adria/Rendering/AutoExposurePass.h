#pragma once
#include "PostEffect.h"
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class RenderGraph;
	class GfxTexture;
	class GfxBuffer;
	class GfxDevice;
	class GfxComputePipelineState;

	class AutoExposurePass : public PostEffect
	{
	public:
		AutoExposurePass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void OnSceneInitialized() override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxTexture> luminance_texture;
		std::unique_ptr<GfxBuffer> histogram_copy;
		Bool invalid_history = true;

		std::unique_ptr<GfxComputePipelineState> build_histogram_pso;
		std::unique_ptr<GfxComputePipelineState> histogram_reduction_pso;
		std::unique_ptr<GfxComputePipelineState> exposure_pso;
		Bool show_histogram		= false;

	private:
		void CreatePSOs();
	};
}


