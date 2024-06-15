#include "FSR3Pass.h"
#include "FidelityFX/gpu/fsr3/ffx_fsr3_resources.h"
#include "FidelityFXUtils.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Logging/Logger.h"

namespace adria
{
	namespace
	{
		void FSR3Log(FfxMsgType type, wchar_t const* message)
		{
			std::string msg = ToString(message);
			switch (type)
			{
			case FFX_MESSAGE_TYPE_WARNING:
				ADRIA_LOG(WARNING, msg.c_str());
				break;
			case FFX_MESSAGE_TYPE_ERROR:
				ADRIA_LOG(ERROR, msg.c_str());
				break;
			default:
				break;
			}
		}
	}

	FSR3Pass::FSR3Pass(GfxDevice* _gfx, uint32 w, uint32 h) : gfx(_gfx), display_width(w), display_height(h), render_width(), render_height()
	{
		if (!gfx->GetCapabilities().SupportsShaderModel(SM_6_6)) return;
		sprintf(name_version, "FSR %d.%d.%d", FFX_FSR3_VERSION_MAJOR, FFX_FSR3_VERSION_MINOR, FFX_FSR3_VERSION_PATCH);
		ffx_interface = CreateFfxInterface(gfx, FFX_FSR3UPSCALER_CONTEXT_COUNT);
		fsr3_context_desc.backendInterfaceUpscaling = *ffx_interface;
		RecreateRenderResolution();
		CreateContext();
	}

	FSR3Pass::~FSR3Pass()
	{
		DestroyContext();
		DestroyFfxInterface(ffx_interface);
	}

	RGResourceName FSR3Pass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		if (recreate_context)
		{
			DestroyContext();
			CreateContext();
		}

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct FSR3PassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId velocity;
			RGTextureReadOnlyId exposure;
			RGTextureReadWriteId output;
		};
		rg.AddPass<FSR3PassData>(name_version,
			[=](FSR3PassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc fsr3_desc{};
				fsr3_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				fsr3_desc.width = display_width;
				fsr3_desc.height = display_height;
				fsr3_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(FSR3Output), fsr3_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(FSR3Output));
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);
				data.velocity = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](FSR3PassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture& input_texture = ctx.GetTexture(*data.input);
				GfxTexture& velocity_texture = ctx.GetTexture(*data.velocity);
				GfxTexture& depth_texture = ctx.GetTexture(*data.depth);
				GfxTexture& output_texture = ctx.GetTexture(*data.output);

				FfxFsr3DispatchUpscaleDescription dispatch_desc{};
				dispatch_desc.commandList = ffxGetCommandListDX12(cmd_list->GetNative());
				dispatch_desc.color = GetFfxResource(input_texture);
				dispatch_desc.depth = GetFfxResource(depth_texture);
				dispatch_desc.motionVectors = GetFfxResource(velocity_texture);
				dispatch_desc.exposure = GetFfxResource();
				dispatch_desc.reactive = GetFfxResource();
				dispatch_desc.transparencyAndComposition = GetFfxResource();
				dispatch_desc.upscaleOutput = GetFfxResource(output_texture, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
				dispatch_desc.jitterOffset.x = frame_data.camera_jitter_x;
				dispatch_desc.jitterOffset.y = frame_data.camera_jitter_y;
				dispatch_desc.motionVectorScale.x = (float)render_width;
				dispatch_desc.motionVectorScale.y = (float)render_height;
				dispatch_desc.reset = false;
				dispatch_desc.enableSharpening = sharpening_enabled;
				dispatch_desc.sharpness = sharpness;
				dispatch_desc.frameTimeDelta = frame_data.frame_delta_time;
				dispatch_desc.preExposure = 1.0f;
				dispatch_desc.renderSize.width = render_width;
				dispatch_desc.renderSize.height = render_height;
				dispatch_desc.cameraFar = frame_data.camera_far;
				dispatch_desc.cameraNear = frame_data.camera_near;
				dispatch_desc.cameraFovAngleVertical = frame_data.camera_fov;

				FfxErrorCode error_code = ffxFsr3ContextDispatchUpscale(&fsr3_context, &dispatch_desc);
				ADRIA_ASSERT(error_code == FFX_OK);

				cmd_list->ResetState();
			}, RGPassType::Compute);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx(name_version, ImGuiTreeNodeFlags_None))
				{
					if (ImGui::Combo("Quality Mode", (int32*)&fsr3_quality_mode, "Custom\0Quality (1.5x)\0Balanced (1.7x)\0Performance (2.0x)\0Ultra Performance (3.0x)\0", 5))
					{
						RecreateRenderResolution();
						recreate_context = true;
					}

					if (fsr3_quality_mode == 0)
					{
						if (ImGui::SliderFloat("Upscale Ratio", &custom_upscale_ratio, 1.0, 3.0))
						{
							RecreateRenderResolution();
							recreate_context = true;
						}
					}
					ImGui::Checkbox("Enable sharpening", &sharpening_enabled);
					if(sharpening_enabled) ImGui::SliderFloat("Sharpness", &sharpness, 0.0f, 1.0f, "%.2f");
					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessing);

		return RG_RES_NAME(FSR3Output);
	}

	void FSR3Pass::CreateContext()
	{
		fsr3_context_desc.fpMessage = FSR3Log;
		fsr3_context_desc.maxRenderSize.width = render_width;
		fsr3_context_desc.maxRenderSize.height = render_height;
		fsr3_context_desc.upscaleOutputSize.width = display_width;
		fsr3_context_desc.upscaleOutputSize.height = display_height;
		fsr3_context_desc.flags = FFX_FSR3_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR3_ENABLE_AUTO_EXPOSURE | FFX_FSR3_ENABLE_UPSCALING_ONLY;
		FfxErrorCode error_code = ffxFsr3ContextCreate(&fsr3_context, &fsr3_context_desc);
		ADRIA_ASSERT(error_code == FFX_OK);
		recreate_context = false;
	}

	void FSR3Pass::DestroyContext()
	{
		gfx->WaitForGPU();
		ffxFsr3ContextDestroy(&fsr3_context);
	}

	void FSR3Pass::RecreateRenderResolution()
	{
		float upscale_ratio = (fsr3_quality_mode == 0 ? custom_upscale_ratio : ffxFsr3GetUpscaleRatioFromQualityMode(fsr3_quality_mode));
		render_width = (uint32)((float)display_width / upscale_ratio);
		render_height = (uint32)((float)display_height / upscale_ratio);
		render_resolution_changed_event.Broadcast(render_width, render_height);
	}

}

