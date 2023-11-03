#define NV_WINDOWS
#include "DLSS3Pass.h"
#include "nvsdk_ngx_helpers.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Logging/Logger.h"


namespace adria
{
	static constexpr char const* project_guid = "a0f57b54-1daf-4934-90ae-c4035c19df04";

	namespace
	{
		void NVSDK_CONV DLSS3Log(char const* message, NVSDK_NGX_Logging_Level logging_level, NVSDK_NGX_Feature source_component)
		{
			switch (logging_level)
			{
			case NVSDK_NGX_LOGGING_LEVEL_VERBOSE:
				ADRIA_LOG(INFO, message);
				break;
			case NVSDK_NGX_LOGGING_LEVEL_ON:
				ADRIA_LOG(WARNING, message);
				break;
			case NVSDK_NGX_LOGGING_LEVEL_OFF:
			default:
				break;
			}
		}
	}

	DLSS3Pass::DLSS3Pass(GfxDevice* gfx, uint32 w, uint32 h) 
		: gfx(gfx), display_width(), display_height(), render_width(), render_height()
	{
		sprintf(name_version, "DLSS 3.5");
		is_supported = InitializeNVSDK_NGX();
	}

	DLSS3Pass::~DLSS3Pass()
	{
		if (ngx_parameters)
		{
			NVSDK_NGX_D3D12_DestroyParameters(ngx_parameters);
		}
		NVSDK_NGX_D3D12_Shutdown1(gfx->GetDevice());
	}

	RGResourceName DLSS3Pass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		return RG_RES_NAME(DLSS3Output);
	}

	bool DLSS3Pass::InitializeNVSDK_NGX()
	{
		if (gfx->GetVendor() != GfxVendor::Nvidia) return false;

		ID3D12Device* device = gfx->GetDevice();

		static const wchar_t* dll_paths[] = 
		{ 
		L"C:\\Users\\Mate\\Desktop\\Projekti\\Adria-DX12\\External\\DLSS\\lib\\dev",
		L"C:\\Users\\Mate\\Desktop\\Projekti\\Adria-DX12\\External\\DLSS\\lib\\rel",
		};

		NVSDK_NGX_FeatureCommonInfo feature_common_info = {};
		feature_common_info.LoggingInfo.LoggingCallback = DLSS3Log;
		feature_common_info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
		feature_common_info.LoggingInfo.DisableOtherLoggingSinks = true;
		feature_common_info.PathListInfo.Path = dll_paths;
		feature_common_info.PathListInfo.Length = NVSDK_NGX_ARRAY_LEN(dll_paths);

		NVSDK_NGX_Result result = NVSDK_NGX_D3D12_Init_with_ProjectID(
			project_guid,
			NVSDK_NGX_ENGINE_TYPE_CUSTOM,
			"1.0",
			L".",
			device,
			&feature_common_info,
			NVSDK_NGX_Version_API);

		result = NVSDK_NGX_D3D12_GetCapabilityParameters(&ngx_parameters);
		if (NVSDK_NGX_FAILED(result)) return false;

		int needs_updated_driver = 0;
		unsigned int min_driver_version_major = 0;
		unsigned int min_driver_version_minor = 0;
		NVSDK_NGX_Result result_updated_driver = ngx_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needs_updated_driver);
		NVSDK_NGX_Result result_min_driver_version_major = ngx_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &min_driver_version_major);
		NVSDK_NGX_Result result_min_driver_version_minor = ngx_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &min_driver_version_minor);
		if (NVSDK_NGX_SUCCEED(result_updated_driver))
		{
			if (needs_updated_driver)
			{
				if (NVSDK_NGX_SUCCEED(result_min_driver_version_major) &&
					NVSDK_NGX_SUCCEED(result_min_driver_version_minor))
				{
					ADRIA_LOG(WARNING, "Nvidia DLSS cannot be loaded due to outdated driver, min driver version: %ul.%ul", min_driver_version_major, min_driver_version_minor);
					return false;
				}
				ADRIA_LOG(WARNING, "Nvidia DLSS cannot be loaded due to outdated driver");
			}
		}

		int dlss_available = 0;
		result = ngx_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlss_available);
		if (NVSDK_NGX_FAILED(result) || !dlss_available) return false;

		return true;
	}

	void DLSS3Pass::RecreateRenderResolution()
	{
		if (perf_quality == NVSDK_NGX_PerfQuality_Value_UltraQuality)
		{
			render_width = (uint32)((float)display_width / custom_upscale_ratio);
			render_height = (uint32)((float)display_height / custom_upscale_ratio);
		}

		unsigned int optimal_width;
		unsigned int optimal_height;
		unsigned int max_width;
		unsigned int max_height;
		unsigned int min_width;
		unsigned int min_height;
		float sharpness;
		NGX_DLSS_GET_OPTIMAL_SETTINGS(ngx_parameters, display_width, display_height, perf_quality, 
									  &optimal_width, &optimal_height, &max_width, &max_height, &min_width, &min_height, &sharpness);
		render_width = optimal_width;
		render_height = optimal_height;
	}

}

