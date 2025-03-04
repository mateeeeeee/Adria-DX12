#include "GfxNsightPerfManager.h"
#if defined(GFX_ENABLE_NV_PERF)
#include <filesystem>
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "Core/Paths.h"
#include "Core/CommandLineOptions.h"
#include "nvperf_host_impl.h"
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "NvPerfUtility/imports/rapidyaml-0.4.0/ryml_all.hpp"
#endif

namespace adria
{
#if defined(GFX_ENABLE_NV_PERF)
	class GfxNsightPerfReporter
	{
	public:
		explicit GfxNsightPerfReporter(GfxDevice* gfx, Bool active) : gfx(gfx), active(active),
			generate_report_command("nsight.perf.report", "Generate Nsight Perf HTML report", ConsoleCommandDelegate::CreateMember(&GfxNsightPerfReporter::GenerateReport, *this))
		{
			if (active)
			{
				if (!report_generator.InitializeReportGenerator(gfx->GetDevice()))
				{
					ADRIA_WARNING("NsightPerf Report Generator Initalization failed, check the VS Output View for NVPERF logs");
					return;
				}
				report_generator.SetFrameLevelRangeName("Frame");
				report_generator.SetNumNestingLevels(3);
				report_generator.SetMaxNumRanges(128);
				report_generator.outputOptions.directoryName = paths::NsightPerfReportDir;
				std::filesystem::create_directory(paths::NsightPerfReportDir);

				clock_info = nv::perf::D3D12GetDeviceClockState(gfx->GetDevice());
				nv::perf::D3D12SetDeviceClockState(gfx->GetDevice(), NVPW_DEVICE_CLOCK_SETTING_DEFAULT);
			}
		}
		~GfxNsightPerfReporter()
		{
			if(active) report_generator.Reset();
		}

		void Update()
		{
			if (active && generate_report)
			{
				if (!report_generator.StartCollectionOnNextFrame())
				{
					ADRIA_WARNING("Nsight Perf: Report Generator Start Collection failed. Please check the logs.");
				}
				generate_report = false;
			}
		}
		void BeginFrame()
		{
			if (active) report_generator.OnFrameStart(gfx->GetGraphicsCommandQueue());
		}
		void EndFrame()
		{
			if (active)
			{
				report_generator.OnFrameEnd();
				if (report_generator.IsCollectingReport())
				{
					ADRIA_INFO("Nsight Perf: Currently profiling the frame. HTML Report will be written to: %s", paths::NsightPerfReportDir.c_str());
				}
				if (report_generator.GetInitStatus() != nv::perf::profiler::ReportGeneratorInitStatus::Succeeded)
				{
					ADRIA_WARNING("Nsight Perf: Initialization failed. Please check the logs.");
				}
			}
		}
		void GenerateReport()
		{
			generate_report = true;
		}
		void PushRange(GfxCommandList* cmd_list, Char const* name)
		{
			if (!active) return;
			report_generator.rangeCommands.PushRange(cmd_list->GetNative(), name);
		}
		void PopRange(GfxCommandList* cmd_list)
		{
			if (!active) return;
			report_generator.rangeCommands.PopRange(cmd_list->GetNative());
		}

	private:
		GfxDevice* gfx;
		Bool active;
		nv::perf::profiler::ReportGeneratorD3D12 report_generator;
		nv::perf::ClockInfo clock_info;
		Bool generate_report = false;
		AutoConsoleCommand generate_report_command;
	};

	class GfxNsightPerfHUD
	{
#ifdef _DEBUG
		static constexpr Uint32 SamplingFrequency = 30;
#else
		static constexpr Uint32 SamplingFrequency = 60;
#endif
	public:
		GfxNsightPerfHUD(GfxDevice* gfx, Bool active) : active(active)
		{
			if (active)
			{
				if (!periodic_sampler.Initialize(gfx->GetDevice()))
				{
					ADRIA_WARNING("NsightPerf Periodic Sampler Initalization failed, check the VS Output View for NVPERF logs");
					return;
				}
				const nv::perf::DeviceIdentifiers device_identifiers = periodic_sampler.GetGpuDeviceIdentifiers();
				static constexpr Uint32 SamplingIntervalInNanoSeconds = 1000 * 1000 * 1000 / SamplingFrequency;
				static constexpr Uint32 MaxDecodeLatencyInNanoSeconds = 1000 * 1000 * 1000;
				if (!periodic_sampler.BeginSession(gfx->GetGraphicsCommandQueue(), SamplingIntervalInNanoSeconds, MaxDecodeLatencyInNanoSeconds, GFX_BACKBUFFER_COUNT))
				{
					ADRIA_WARNING("NsightPerf Periodic Sampler BeginSession failed, check the VS Output View for NVPERF logs");
					return;
				}

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
				hud_renderer.Initialize(hud_data_model);
			}
		}
		~GfxNsightPerfHUD()
		{
			if (active) periodic_sampler.Reset();
		}

		void Update()
		{
			if (active)
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
		}
		void BeginFrame()
		{
		}
		void Render()
		{
			if (active) hud_renderer.Render();
		}
		void EndFrame()
		{
			if (active) periodic_sampler.OnFrameEnd();
		}

	private:
		Bool active = false;
		nv::perf::sampler::PeriodicSamplerTimeHistoryD3D12 periodic_sampler;
		nv::perf::hud::HudDataModel hud_data_model;
		nv::perf::hud::HudImPlotRenderer hud_renderer;
	};

	GfxNsightPerfManager::GfxNsightPerfManager(GfxDevice* gfx, GfxNsightPerfMode perf_mode)
	{
		perf_reporter = std::make_unique<GfxNsightPerfReporter>(gfx, perf_mode == GfxNsightPerfMode::HTMLReport);
		perf_hud = std::make_unique<GfxNsightPerfHUD>(gfx, perf_mode == GfxNsightPerfMode::HUD);
	}
	GfxNsightPerfManager::~GfxNsightPerfManager() = default;

	void GfxNsightPerfManager::Update()
	{
		perf_hud->Update();
		perf_reporter->Update();
	}
	void GfxNsightPerfManager::BeginFrame()
	{
		perf_hud->BeginFrame();
		perf_reporter->BeginFrame();
	}
	void GfxNsightPerfManager::Render()
	{
		perf_hud->Render();
	}
	void GfxNsightPerfManager::EndFrame()
	{
		perf_hud->EndFrame();
		perf_reporter->EndFrame();
	}
	void GfxNsightPerfManager::PushRange(GfxCommandList* cmd_list, Char const* name)
	{
		perf_reporter->PushRange(cmd_list, name);
	}
	void GfxNsightPerfManager::PopRange(GfxCommandList* cmd_list)
	{
		perf_reporter->PopRange(cmd_list);
	}
	void GfxNsightPerfManager::GenerateReport()
	{
		perf_reporter->GenerateReport();
	}
#else
	GfxNsightPerfManager::GfxNsightPerfManager(GfxDevice* gfx, GfxNsightPerfMode perf_mode) {}
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