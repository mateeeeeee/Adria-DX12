#pragma once
#include "nvsdk_ngx_defs.h"
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"


struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class RenderGraph;

	class DLSS3Pass : public UpscalerPass
	{
	public:
		DLSS3Pass(GfxDevice* gfx, uint32 w, uint32 h);
		~DLSS3Pass();

		RGResourceName AddPass(RenderGraph& rg, RGResourceName input);

		virtual void OnResize(uint32 w, uint32 h) override
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			needs_create = true;
		}
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

		virtual bool IsSupported() const override { return is_supported; }

	private:
		bool is_supported = false;
		char name_version[16] = {};
		GfxDevice* gfx = nullptr;
		uint32 display_width, display_height;
		uint32 render_width, render_height;

		NVSDK_NGX_Parameter* ngx_parameters = nullptr;
		NVSDK_NGX_Handle* dlss_feature = nullptr;
		bool needs_create = true;

		NVSDK_NGX_PerfQuality_Value perf_quality = NVSDK_NGX_PerfQuality_Value_Balanced;
		float sharpness = 0.5f;

	private:
		bool InitializeNVSDK_NGX();
		void RecreateRenderResolution();

		void CreateDLSS(GfxCommandList* cmd_list);
		void ReleaseDLSS();
	};
}