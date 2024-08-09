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
		FSR3Pass(GfxDevice* gfx, uint32 w, uint32 h);
		~FSR3Pass();

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
		FfxFsr3ContextDescription fsr3_context_desc{};
		FfxFsr3Context fsr3_context{};
		bool recreate_context = false;

		FfxFsr3QualityMode fsr3_quality_mode = FFX_FSR3_QUALITY_MODE_QUALITY;
		float custom_upscale_ratio = 1.0f;
		bool  sharpening_enabled = false;
		float sharpness = 0.5f;

	private:
		void CreateContext();
		void DestroyContext();
		void RecreateRenderResolution();
	};
}