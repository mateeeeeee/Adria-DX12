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
				ADRIA_LOG(DEBUG, message);
				break;
			case NVSDK_NGX_LOGGING_LEVEL_ON:
				ADRIA_LOG(INFO, message);
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
		if(!IsSupported()) ADRIA_ASSERT_MSG(false, "DLSS is not supported on this device");

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct DLSS3PassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId velocity;
			RGTextureReadOnlyId exposure;
			RGTextureReadWriteId output;
		};

		rg.AddPass<DLSS3PassData>(name_version,
			[=](DLSS3PassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc dlss3_desc{};
				dlss3_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				dlss3_desc.width = display_width;
				dlss3_desc.height = display_height;
				dlss3_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(DLSS3Output), dlss3_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(DLSS3Output));
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);
				data.velocity = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](DLSS3PassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				if (needs_create) CreateDLSS(cmd_list);

				GfxTexture& input_texture = ctx.GetTexture(*data.input);
				GfxTexture& velocity_texture = ctx.GetTexture(*data.velocity);
				GfxTexture& depth_texture = ctx.GetTexture(*data.depth);
				GfxTexture& output_texture = ctx.GetTexture(*data.output);

				NVSDK_NGX_D3D12_DLSS_Eval_Params dlss_eval_params{};
				dlss_eval_params.Feature.pInColor = input_texture.GetNative();
				dlss_eval_params.pInDepth = depth_texture.GetNative();
				dlss_eval_params.pInMotionVectors = velocity_texture.GetNative();
				dlss_eval_params.Feature.pInOutput = output_texture.GetNative();
				dlss_eval_params.pInExposureTexture = nullptr;
				dlss_eval_params.InJitterOffsetX = frame_data.camera_jitter_x;
				dlss_eval_params.InJitterOffsetY = frame_data.camera_jitter_y;
				dlss_eval_params.Feature.InSharpness = sharpness;
				dlss_eval_params.InReset = false;
				dlss_eval_params.InExposureScale = 1.0f;
				dlss_eval_params.InMVScaleX = (float)render_width;
				dlss_eval_params.InMVScaleY = (float)render_height;
				dlss_eval_params.InRenderSubrectDimensions = { render_width, render_height };

				NVSDK_NGX_Result result = NGX_D3D12_EVALUATE_DLSS_EXT((ID3D12GraphicsCommandList*)cmd_list->GetNative(), dlss_feature, ngx_parameters, &dlss_eval_params);
				ADRIA_ASSERT(NVSDK_NGX_SUCCEED(result));

				cmd_list->ResetState();
			}, RGPassType::Compute);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx(name_version, ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Combo("Performance Quality", (int*)&perf_quality, "Quality (1.5x)\0Balanced (1.7x)\0Performance (2.0x)\0Ultra Performance (3.0x)\0UltraQuality\0", 5))
					{
						RecreateRenderResolution();
						needs_create = true;
					}

					ImGui::SliderFloat("Sharpness", &sharpness, 0.0f, 1.0f, "%.2f");
					ImGui::TreePop();
				}
			});

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
		feature_common_info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
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
		unsigned int optimal_width;
		unsigned int optimal_height;
		unsigned int max_width;
		unsigned int max_height;
		unsigned int min_width;
		unsigned int min_height;
		float sharpness;
		NGX_DLSS_GET_OPTIMAL_SETTINGS(ngx_parameters, display_width, display_height, perf_quality, 
									  &optimal_width, &optimal_height, &max_width, &max_height, &min_width, &min_height, &sharpness);
		ADRIA_ASSERT(optimal_width != 0 && optimal_height != 0);
		render_width = optimal_width;
		render_height = optimal_height;
		render_resolution_changed_event.Broadcast(render_width, render_height);
	}

	void DLSS3Pass::CreateDLSS(GfxCommandList* cmd_list)
	{
		if (!IsSupported()) return;

		ReleaseDLSS();
		if (needs_create)
		{
			NVSDK_NGX_DLSS_Create_Params dlss_create_params = {};
			dlss_create_params.Feature.InWidth = render_width;
			dlss_create_params.Feature.InHeight = render_height;
			dlss_create_params.Feature.InTargetWidth = display_width;
			dlss_create_params.Feature.InTargetHeight = display_height;
			dlss_create_params.Feature.InPerfQualityValue = perf_quality;
			dlss_create_params.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
													  NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
													  NVSDK_NGX_DLSS_Feature_Flags_AutoExposure |
													  NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;

			NVSDK_NGX_Result result = NGX_D3D12_CREATE_DLSS_EXT(cmd_list->GetNative(), 0, 0, &dlss_feature, ngx_parameters, &dlss_create_params);
			ADRIA_ASSERT(NVSDK_NGX_SUCCEED(result));
			cmd_list->UavBarrier();
			needs_create = false;
		}
	}

	void DLSS3Pass::ReleaseDLSS()
	{
		if (dlss_feature)
		{
			gfx->WaitForGPU();
			NVSDK_NGX_D3D12_ReleaseFeature(dlss_feature);
			dlss_feature = nullptr;
		}
	}

}

