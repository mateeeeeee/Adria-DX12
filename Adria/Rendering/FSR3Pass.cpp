#include "FSR3Pass.h"
#include "FidelityFX/gpu/fsr3upscaler/ffx_fsr3upscaler_resources.h"
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
		void FSR3Log(FfxMsgType type, const wchar_t* message)
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

	FSR3Pass::FSR3Pass(GfxDevice* _gfx, FfxInterface& ffx_interface, uint32 w, uint32 h)
		: gfx(_gfx), display_width(w), display_height(h), render_width(), render_height()
	{
		if (!gfx->GetCapabilities().SupportsShaderModel(SM_6_6)) return;
		sprintf(name_version, "FSR %d.%d.%d", FFX_FSR3UPSCALER_VERSION_MAJOR, FFX_FSR3UPSCALER_VERSION_MINOR, FFX_FSR3UPSCALER_VERSION_PATCH);
		fsr3_context_desc.backendInterface = ffx_interface;
		RecreateRenderResolution();
		CreateContext();
	}

	FSR3Pass::~FSR3Pass()
	{
		DestroyContext();
	}

	RGResourceName FSR3Pass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		ADRIA_ASSERT_MSG(false, "Not implemented yet!");
		return RGResourceName();
	}

	void FSR3Pass::CreateContext()
	{
		fsr3_context_desc.fpMessage = FSR3Log;
		fsr3_context_desc.maxRenderSize.width = render_width;
		fsr3_context_desc.maxRenderSize.height = render_height;
		fsr3_context_desc.displaySize.width = display_width;
		fsr3_context_desc.displaySize.height = display_height;
		fsr3_context_desc.flags = FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR3UPSCALER_ENABLE_AUTO_EXPOSURE;

		FfxErrorCode error_code = ffxFsr3UpscalerContextCreate(&fsr3_context, &fsr3_context_desc);
		ADRIA_ASSERT(error_code == FFX_OK);
		recreate_context = false;
	}

	void FSR3Pass::DestroyContext()
	{
		gfx->WaitForGPU();
		ffxFsr3UpscalerContextDestroy(&fsr3_context);
	}

	void FSR3Pass::RecreateRenderResolution()
	{
		float upscale_ratio = (fsr3_quality_mode == 0 ? custom_upscale_ratio : ffxFsr3UpscalerGetUpscaleRatioFromQualityMode(fsr3_quality_mode));
		render_width = (uint32)((float)display_width / upscale_ratio);
		render_height = (uint32)((float)display_height / upscale_ratio);
		render_resolution_changed_event.Broadcast(render_width, render_height);
	}

}

