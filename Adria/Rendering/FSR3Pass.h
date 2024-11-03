#pragma once
#include "FidelityFX/host/ffx_fsr3.h"
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FSR3Pass : public UpscalerPass
	{
	public:
		FSR3Pass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~FSR3Pass();

		virtual void OnResize(Uint32 w, Uint32 h) override
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			DestroyContext();
			CreateContext();
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		Char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		Uint32 display_width, display_height;
		Uint32 render_width, render_height;

		FfxInterface* ffx_interface;
		FfxFsr3ContextDescription fsr3_context_desc{};
		FfxFsr3Context fsr3_context{};
		Bool recreate_context = false;

		FfxFsr3QualityMode fsr3_quality_mode = FFX_FSR3_QUALITY_MODE_QUALITY;
		Float custom_upscale_ratio = 1.0f;
		Bool  sharpening_enabled = false;
		Float sharpness = 0.5f;

	private:
		void CreateContext();
		void DestroyContext();
		void RecreateRenderResolution();
	};
}