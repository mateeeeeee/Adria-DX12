#pragma once
#include "nvsdk_ngx_defs.h"
#include "RenderGraph/RenderGraphResourceName.h"
#include "Utilities/Delegate.h"


struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class RenderGraph;

	class DLSS3Pass
	{
		DECLARE_EVENT(RenderResolutionChanged, DLSS3Pass, uint32, uint32)
	public:
		DLSS3Pass(GfxDevice* gfx, uint32 w, uint32 h);
		~DLSS3Pass();

		RGResourceName AddPass(RenderGraph& rg, RGResourceName input);

		void OnResize(uint32 w, uint32 h)
		{
			display_width = w, display_height = h;
			RecreateRenderResolution();
			needs_create = true;
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

		NVSDK_NGX_Parameter* ngx_parameters = nullptr;
		NVSDK_NGX_Handle* dlss_feature = nullptr;
		bool needs_create = true;

		NVSDK_NGX_PerfQuality_Value perf_quality = NVSDK_NGX_PerfQuality_Value_Balanced;
		float sharpness = 0.5f;

		RenderResolutionChanged render_resolution_changed_event;

	private:
		bool InitializeNVSDK_NGX();
		void RecreateRenderResolution();

		void CreateDLSS(GfxCommandList* cmd_list);
		void ReleaseDLSS();
	};
}