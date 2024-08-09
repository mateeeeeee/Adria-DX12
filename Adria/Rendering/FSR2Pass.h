#pragma once
#include "FidelityFX/host/ffx_fsr2.h"
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FSR2Pass : public UpscalerPass
	{
	public:
		FSR2Pass(GfxDevice* gfx, uint32 w, uint32 h);
		~FSR2Pass();

		virtual void OnResize(uint32 w, uint32 h) override
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			DestroyContext();
			CreateContext();
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		uint32 display_width, display_height;
		uint32 render_width, render_height;

		FfxInterface* ffx_interface;
		FfxFsr2ContextDescription fsr2_context_desc{};
		FfxFsr2Context fsr2_context{};
		bool recreate_context = false;

		FfxFsr2QualityMode fsr2_quality_mode = FFX_FSR2_QUALITY_MODE_QUALITY;
		float custom_upscale_ratio = 1.0f;
		float sharpness = 0.5f;

	private:
		void CreateContext();
		void DestroyContext();
		void RecreateRenderResolution();
	};
}