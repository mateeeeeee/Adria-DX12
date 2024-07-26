#pragma once
#include "FidelityFX/host/ffx_fsr3.h"
#include "RenderGraph/RenderGraphResourceName.h"
#include "Utilities/Delegate.h"

struct FfxInterface;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class FSR3Pass
	{
		DECLARE_EVENT(RenderResolutionChanged, FSR3Pass, uint32, uint32)

	public:
		FSR3Pass(GfxDevice* gfx, uint32 w, uint32 h);
		~FSR3Pass();

		RGResourceName AddPass(RenderGraph& rg, RGResourceName input);

		void OnResize(uint32 w, uint32 h)
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			DestroyContext();
			CreateContext();
		}

		Vector2u GetRenderResolution()  const { return Vector2u(render_width, render_height); }
		Vector2u GetDisplayResolution() const { return Vector2u(display_width, display_height); }

		RenderResolutionChanged& GetRenderResolutionChangedEvent() { return render_resolution_changed_event; }

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

		RenderResolutionChanged render_resolution_changed_event;
	private:

		void SetCVarCallbacks();
		void CreateContext();
		void DestroyContext();
		void RecreateRenderResolution();
	};
}