#pragma once
#include "GfxMacros.h"
#if defined(GFX_ENABLE_NV_PERF)
#include "Core/ConsoleManager.h"
#include "NvPerfUtility/include/NvPerfPeriodicSamplerD3D12.h"
#include "NvPerfUtility/include/NvPerfMetricConfigurationsHAL.h"
#include "NvPerfUtility/include/NvPerfHudDataModel.h"
#include "NvPerfUtility/include/NvPerfHudImPlotRenderer.h"
#include "NvPerfUtility/include/NvPerfReportGeneratorD3D12.h"
#endif

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class GfxNsightPerfReporter;
	class GfxNsightPerfHUD;

	enum class GfxNsightPerfMode
	{
		HTMLReport,
		HUD
	};
	class GfxNsightPerfManager
	{
	public:
		GfxNsightPerfManager(GfxDevice* gfx, GfxNsightPerfMode perf_mode);
		~GfxNsightPerfManager();

		void Update();
		void BeginFrame();
		void Render();
		void EndFrame();
		void PushRange(GfxCommandList*, Char const*);
		void PopRange(GfxCommandList*);
		void GenerateReport();

	private:
#if defined(GFX_ENABLE_NV_PERF)
		std::unique_ptr<GfxNsightPerfReporter> perf_reporter;
		std::unique_ptr<GfxNsightPerfHUD>      perf_hud;
#endif
	};

#if defined(GFX_ENABLE_NV_PERF)
	struct GfxNsightPerfRangeScope
	{
		GfxNsightPerfRangeScope(GfxCommandList* _cmd_list, Char const* _name);
		~GfxNsightPerfRangeScope();
		GfxCommandList* cmd_list;
		std::string name;
	};
#define NsightPerfGfxRangeScope(cmd_list, name) GfxNsightPerfRangeScope ADRIA_CONCAT(scope, __COUNTER__)(cmd_list, name)
#else
#define NsightPerfGfxRangeScope(cmd_list, name) 
#endif
}