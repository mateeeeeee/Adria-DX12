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

	class GfxNsightPerfManager
	{
#ifdef _DEBUG
		static constexpr Uint32 SamplingFrequency = 30;
#else
		static constexpr Uint32 SamplingFrequency = 60;
#endif
	public:
		explicit GfxNsightPerfManager(GfxDevice* gfx);
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
		GfxDevice* gfx;
		nv::perf::sampler::PeriodicSamplerTimeHistoryD3D12 periodic_sampler;
		nv::perf::hud::HudDataModel hud_data_model;
		nv::perf::hud::HudImPlotRenderer hud_renderer;
		nv::perf::profiler::ReportGeneratorD3D12 report_generator;
		nv::perf::ClockInfo clock_info; 
		Bool generate_report = false;
		AutoConsoleCommand generate_report_command;
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