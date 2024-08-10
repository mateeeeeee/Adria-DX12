#include "XeSSPass.h"
#include "XeSS/xess_d3d12.h"
#include "BlackboardData.h"
#include "PostProcessor.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"
#include "Logging/Logger.h"

namespace adria
{
	static TAutoConsoleVariable<bool> XeSS("r.XeSS", true, "Enable or Disable XeSS");

	namespace
	{
		void XeSSLog(char const* message, xess_logging_level_t logging_level)
		{
			switch (logging_level)
			{
			case XESS_LOGGING_LEVEL_DEBUG:
				ADRIA_LOG(DEBUG, message);
				break;
			case XESS_LOGGING_LEVEL_INFO:
				ADRIA_LOG(INFO, message);
				break;
			case XESS_LOGGING_LEVEL_WARNING:
				ADRIA_LOG(WARNING, message);
				break;
			case XESS_LOGGING_LEVEL_ERROR:
				ADRIA_LOG(ERROR, message);
				break;
			default:
				break;
			}
		}
	}
	

	XeSSPass::XeSSPass(GfxDevice* gfx, uint32 w, uint32 h) 
		: gfx(gfx), display_width(), display_height(), render_width(), render_height()
	{
		if (!gfx->GetCapabilities().SupportsRayTracing()) return;

		xess_result_t result = xessD3D12CreateContext(gfx->GetDevice(), &context);
		ADRIA_ASSERT(result == XESS_RESULT_SUCCESS);
		
		xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_DEBUG, XeSSLog);

		xess_version_t version;
		xessGetVersion(&version);
		sprintf(name_version, "XeSS %d.%d.%d", version.major, version.minor, version.patch);

		OnResize(w, h);

	}

	XeSSPass::~XeSSPass()
	{
		gfx->WaitForGPU();
		xessDestroyContext(context);
	}

	void XeSSPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		if (needs_init) XeSSInit();

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct XeSSPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId velocity;
			RGTextureReadOnlyId exposure;
			RGTextureReadWriteId output;
		};

		rg.AddPass<XeSSPassData>(name_version,
			[=](XeSSPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc xess_desc{};
				xess_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				xess_desc.width = display_width;
				xess_desc.height = display_height;
				xess_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_NAME(XeSSOutput), xess_desc);

				data.output = builder.WriteTexture(RG_NAME(XeSSOutput));
				data.input = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
				data.velocity = builder.ReadTexture(RG_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](XeSSPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				if (needs_init) XeSSInit();

				GfxTexture& input_texture = ctx.GetTexture(*data.input);
				GfxTexture& velocity_texture = ctx.GetTexture(*data.velocity);
				GfxTexture& depth_texture = ctx.GetTexture(*data.depth);
				GfxTexture& output_texture = ctx.GetTexture(*data.output);

				xess_d3d12_execute_params_t execute_params{};
				execute_params.pColorTexture = input_texture.GetNative();
				execute_params.pVelocityTexture = velocity_texture.GetNative();
				execute_params.pDepthTexture = depth_texture.GetNative();
				execute_params.pOutputTexture = output_texture.GetNative();
				execute_params.pExposureScaleTexture = nullptr;
				execute_params.jitterOffsetX = frame_data.camera_jitter_x;
				execute_params.jitterOffsetY = frame_data.camera_jitter_y;
				execute_params.exposureScale = 1.0f;
				execute_params.resetHistory = false;
				execute_params.inputWidth = render_width;
				execute_params.inputHeight = render_height;

				xessSetJitterScale(context, 1.0f, 1.0f);
				xessSetVelocityScale(context, (float)render_width, (float)render_height);

				xess_result_t result = xessD3D12Execute(context, cmd_list->GetNative(), &execute_params);
				ADRIA_ASSERT(result == XESS_RESULT_SUCCESS);

				cmd_list->ResetState();
			}, RGPassType::Compute);

		postprocessor->SetFinalResource(RG_NAME(XeSSOutput));
	}

	bool XeSSPass::IsEnabled(PostProcessor const*) const
	{
		return XeSS.Get();
	}

	void XeSSPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx(name_version, ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Enable", XeSS.GetPtr());
					if (XeSS.Get())
					{
						int _quality = quality_setting - XESS_QUALITY_SETTING_PERFORMANCE;
						if (ImGui::Combo("Quality Mode", &_quality, "Performance (2.0x)\0Balanced (1.7x)\0Quality (1.5x)\0Ultra Quality (1.3x)\0\0", 4))
						{
							quality_setting = (xess_quality_settings_t)(_quality + XESS_QUALITY_SETTING_PERFORMANCE);
							needs_init = true;
						}
					}
					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Upscaler);
	}

	void XeSSPass::XeSSInit()
	{
		if (needs_init)
		{
			gfx->WaitForGPU();

			xess_d3d12_init_params_t params{};
			params.outputResolution.x = display_width;
			params.outputResolution.y = display_height;
			params.qualitySetting = quality_setting;
			params.initFlags = XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE | XESS_INIT_FLAG_INVERTED_DEPTH;

			xess_result_t result = xessD3D12Init(context, &params);
			ADRIA_ASSERT(result == XESS_RESULT_SUCCESS);
			needs_init = false;
		}
	}

	void XeSSPass::RecreateRenderResolution()
	{
		xess_2d_t output_resolution = { display_width, display_height };
		xess_2d_t input_resolution;
		xessGetInputResolution(context, &output_resolution, quality_setting, &input_resolution);
		render_width = input_resolution.x;
		render_height = input_resolution.y;
		BroadcastRenderResolutionChanged(render_width, render_height);
	}
}

