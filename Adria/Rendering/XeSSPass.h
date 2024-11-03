#pragma once
#include "XeSS/xess.h"
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	
	class XeSSPass : public UpscalerPass
	{
	public:
		XeSSPass(GfxDevice* gfx, Uint32 w, Uint32 h);
		~XeSSPass();

		virtual void OnResize(Uint32 w, Uint32 h) override
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			needs_init = true;
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		Char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		Uint32 display_width, display_height;
		Uint32 render_width, render_height;

		xess_context_handle_t context{};
		xess_quality_settings_t quality_setting = XESS_QUALITY_SETTING_QUALITY;
		Bool needs_init = false;

	private:
		void XeSSInit();
		void RecreateRenderResolution();
	};
}