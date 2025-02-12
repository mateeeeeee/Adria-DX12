#include "GfxNsightPerfManager.h"
#if defined(GFX_ENABLE_NV_PERF)
#include <filesystem>
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "Core/Paths.h"
#include "Logging/Logger.h"
#include "nvperf_host_impl.h"
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "NvPerfUtility/imports/rapidyaml-0.4.0/ryml_all.hpp"
#endif

namespace adria
{
#if defined(GFX_ENABLE_NV_PERF)

	GfxNsightPerfManager::GfxNsightPerfManager(GfxDevice* gfx) 
		: gfx(gfx),
		  generate_report_command("nsight.perf.report", "Generate Nsight Perf HTML report", ConsoleCommandDelegate::CreateMember(&GfxNsightPerfManager::GenerateReport, *this))
	{
		periodic_sampler.Initialize(gfx->GetDevice());
		const nv::perf::DeviceIdentifiers device_identifiers = periodic_sampler.GetGpuDeviceIdentifiers();
		static constexpr Uint32 SamplingIntervalInNanoSeconds = 1000 * 1000 * 1000 / SamplingFrequency;
		static constexpr Uint32 MaxDecodeLatencyInNanoSeconds = 1000 * 1000 * 1000; // tolerate stutter frame up to 1 second
		periodic_sampler.BeginSession(gfx->GetCommandQueue(GfxCommandListType::Graphics), SamplingIntervalInNanoSeconds, MaxDecodeLatencyInNanoSeconds, GFX_BACKBUFFER_COUNT);

		nv::perf::hud::HudPresets hud_presets;
		hud_presets.Initialize(device_identifiers.pChipName);
		static constexpr Float PlotTimeWidthInSeconds = 4.0;
		hud_data_model.Load(hud_presets.GetPreset("Graphics General Triage"));
		std::string metric_config_name;
		nv::perf::MetricConfigObject metric_config_object;
		if (nv::perf::MetricConfigurations::GetMetricConfigNameBasedOnHudConfigurationName(metric_config_name, device_identifiers.pChipName, "Graphics General Triage"))
		{
			nv::perf::MetricConfigurations::LoadMetricConfigObject(metric_config_object, device_identifiers.pChipName, metric_config_name);
		}
		hud_data_model.Initialize(1.0 / (Float64)SamplingFrequency, PlotTimeWidthInSeconds, metric_config_object);
		periodic_sampler.SetConfig(&hud_data_model.GetCounterConfiguration());
		hud_data_model.PrepareSampleProcessing(periodic_sampler.GetCounterData());

		nv::perf::hud::HudImPlotRenderer::SetStyle();
		hud_renderer.Initialize(hud_data_model);

		report_generator.InitializeReportGenerator(gfx->GetDevice());
		report_generator.SetFrameLevelRangeName("Frame");
		report_generator.SetNumNestingLevels(2);
		report_generator.SetMaxNumRanges(128);
		report_generator.outputOptions.directoryName = paths::NsightPerfReportDir;
		std::filesystem::create_directory(paths::NsightPerfReportDir);

		clock_info = nv::perf::D3D12GetDeviceClockState(gfx->GetDevice());
		nv::perf::D3D12SetDeviceClockState(gfx->GetDevice(), NVPW_DEVICE_CLOCK_SETTING_DEFAULT);
	}

	GfxNsightPerfManager::~GfxNsightPerfManager()
	{
		report_generator.Reset();
		periodic_sampler.Reset();
	}
	void GfxNsightPerfManager::Update()
	{
		periodic_sampler.DecodeCounters();
		periodic_sampler.ConsumeSamples([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop)
		{
			stop = false;
			return hud_data_model.AddSample(pCounterDataImage, counterDataImageSize, rangeIndex);
		});

		for (auto& frame_delimeter : periodic_sampler.GetFrameDelimiters())
		{
			hud_data_model.AddFrameDelimiter(frame_delimeter.frameEndTime);
		}
	}
	void GfxNsightPerfManager::BeginFrame()
	{
		if (generate_report)
		{
			report_generator.StartCollectionOnNextFrame();
			generate_report = false;
		}
		report_generator.OnFrameStart(gfx->GetCommandQueue(GfxCommandListType::Graphics));
	}
	void GfxNsightPerfManager::Render()
	{
		hud_renderer.Render();
	}
	void GfxNsightPerfManager::EndFrame()
	{
		periodic_sampler.OnFrameEnd();
		if (report_generator.IsCollectingReport())
		{
			ADRIA_INFO("Nsight Perf: Currently profiling the frame. HTML Report will be written to: %s", paths::NsightPerfReportDir.c_str());
		}
		if (report_generator.GetInitStatus() != nv::perf::profiler::ReportGeneratorInitStatus::Succeeded)
		{
			ADRIA_WARNING("Nsight Perf: Initialization failed. Please check the logs.");
		}
		report_generator.OnFrameEnd();
	}
	void GfxNsightPerfManager::PushRange(GfxCommandList* cmd_list, Char const* name)
	{
		report_generator.rangeCommands.PushRange(cmd_list->GetNative(), name);
	}
	void GfxNsightPerfManager::PopRange(GfxCommandList* cmd_list)
	{
		report_generator.rangeCommands.PopRange(cmd_list->GetNative());
	}
	void GfxNsightPerfManager::GenerateReport()
	{
		generate_report = true;
	}

	GfxNsightPerfRangeScope::GfxNsightPerfRangeScope(GfxCommandList* _cmd_list, Char const* _name) : name{ _name }, cmd_list{ _cmd_list }
	{
		if (GfxNsightPerfManager* nsight_perf_manager = cmd_list->GetDevice()->GetNsightPerfManager())
		{
			nsight_perf_manager->PushRange(cmd_list, name.c_str());
		}
	}

	GfxNsightPerfRangeScope::~GfxNsightPerfRangeScope()
	{
		if (GfxNsightPerfManager* nsight_perf_manager = cmd_list->GetDevice()->GetNsightPerfManager())
		{
			nsight_perf_manager->PopRange(cmd_list);
		}
	}
#else
	GfxNsightPerfManager::GfxNsightPerfManager(GfxDevice* gfx) {}
	GfxNsightPerfManager::~GfxNsightPerfManager() {}
	void GfxNsightPerfManager::Update() {}
	void GfxNsightPerfManager::BeginFrame() {}
	void GfxNsightPerfManager::Render() {}
	void GfxNsightPerfManager::EndFrame() {}
	void GfxNsightPerfManager::PushRange(GfxCommandList*, Char const*) {}
	void GfxNsightPerfManager::PopRange(GfxCommandList* cmd_list) {}
	void GfxNsightPerfManager::GenerateReport() {}
#endif
}