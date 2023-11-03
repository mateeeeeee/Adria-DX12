#pragma once
#include "FSR2/ffx_fsr2.h"
#include "RenderGraph/RenderGraphResourceName.h"
#include "Events/Delegate.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	
	class FSR2Pass
	{
		DECLARE_EVENT(RenderResolutionChanged, FSR2Pass, uint32, uint32);
	public:
		FSR2Pass(GfxDevice* gfx, uint32 w, uint32 h);
		~FSR2Pass();

		RGResourceName AddPass(RenderGraph& rg, RGResourceName input);
		
		void OnResize(uint32 w, uint32 h)
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			recreate_context = true;
		}

		Vector2u GetRenderResolution()  const { return Vector2u(render_width, render_height); }
		Vector2u GetDisplayResolution() const { return Vector2u(display_width, display_height); }

		RenderResolutionChanged& GetRenderResolutionChangedEvent() { return render_resolution_changed_event; }
		
		bool IsSupported() const { return is_supported; }

	private:
		bool is_supported = false;
		char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		uint32 display_width, display_height;
		uint32 render_width, render_height;

		FfxFsr2ContextDescription context_desc = {};
		FfxFsr2Context context = {};
		bool recreate_context = false;

		FfxFsr2QualityMode quality_mode = FFX_FSR2_QUALITY_MODE_QUALITY;
		float custom_upscale_ratio = 1.0f;
		float sharpness = 0.5f;

		RenderResolutionChanged render_resolution_changed_event;

	private:
		void CreateContext();
		void DestroyContext();
		void RecreateRenderResolution();
	};
}