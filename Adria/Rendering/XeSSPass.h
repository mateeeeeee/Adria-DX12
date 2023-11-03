#pragma once
#include "XeSS/xess.h"
#include "RenderGraph/RenderGraphResourceName.h"
#include "Events/Delegate.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	
	class XeSSPass
	{
		DECLARE_EVENT(RenderResolutionChanged, XeSSPass, uint32, uint32);
	public:
		explicit XeSSPass(GfxDevice* gfx, uint32 w, uint32 h);
		~XeSSPass();

		RGResourceName AddPass(RenderGraph& rg, RGResourceName input);
		void OnResize(uint32 w, uint32 h)
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			needs_init = true;
		}

		Vector2u GetRenderResolution()  const { return Vector2u(render_width, render_height); }
		Vector2u GetDisplayResolution() const { return Vector2u(display_width, display_height); }

		RenderResolutionChanged& GetRenderResolutionChangedEvent() { return render_resolution_changed_event; }

	private:
		char name_version[16];
		GfxDevice* gfx = nullptr;
		uint32 display_width, display_height;
		uint32 render_width, render_height;

		xess_context_handle_t context;
		xess_quality_settings_t quality_setting = XESS_QUALITY_SETTING_QUALITY;
		bool needs_init = false;

		RenderResolutionChanged render_resolution_changed_event;

	private:
		void XeSSInit();
		void RecreateRenderResolution();
	};
}