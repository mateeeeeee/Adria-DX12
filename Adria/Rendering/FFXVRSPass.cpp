#include "FFXVRSPass.h"
#include "FidelityFXUtils.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{

	FFXVRSPass::FFXVRSPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), shading_rate_image_tile_size(0),
		ffx_interface(nullptr), vrs_context{}, vrs_context_description{}
	{
		is_supported = gfx->GetCapabilities().SupportsVSR();
		if (!is_supported)
		{
			return;
		}
		sprintf(name_version, "FFX VRS %d.%d.%d", FFX_VRS_VERSION_MAJOR, FFX_VRS_VERSION_MINOR, FFX_VRS_VERSION_PATCH);
		ffx_interface = CreateFfxInterface(gfx, FFX_VRS_CONTEXT_COUNT);
		vrs_context_description.backendInterface = *ffx_interface;
		ADRIA_ASSERT(vrs_context_description.backendInterface.fpGetSDKVersion(&vrs_context_description.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 0));
		ADRIA_ASSERT(ffxVrsGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 2, 0));

		additional_shading_rates_supported = gfx->GetCapabilities().SupportsAdditionalShadingRates();
		shading_rate_image_tile_size = gfx->GetCapabilities().GetShadingRateImageTileSize();

		CreateContext();
	}

	FFXVRSPass::~FFXVRSPass()
	{
		DestroyContext();
		DestroyFfxInterface(ffx_interface);
	}

	void FFXVRSPass::AddPass(RenderGraph& rg)
	{
		if (!is_supported) return;

		struct FFXVRSPassData
		{
			RGTextureReadOnlyId  color_history;
			RGTextureReadOnlyId  motion_vectors;
			RGTextureReadWriteId vrs_image;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.AddPass<FFXVRSPassData>(name_version,
			[=](FFXVRSPassData& data, RenderGraphBuilder& builder)
			{
				uint32 vrs_image_width, vrs_image_height;
				ffxVrsGetImageSizeFromeRenderResolution(&vrs_image_width, &vrs_image_height, width, height, shading_rate_image_tile_size);
				RGTextureDesc vrs_image_desc{};
				vrs_image_desc.format = GfxFormat::R8_UINT;
				vrs_image_desc.width = vrs_image_width;
				vrs_image_desc.height = vrs_image_height;
				builder.DeclareTexture(RG_NAME(VRSImage), vrs_image_desc);

				data.vrs_image = builder.WriteTexture(RG_NAME(VRSImage));
				data.color_history = builder.ReadTexture(RG_NAME(History), ReadAccess_NonPixelShader);
				data.motion_vectors = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
			},
			[=](FFXVRSPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture& color_history_texture = ctx.GetTexture(*data.color_history);
				GfxTexture& motion_vectors_texture = ctx.GetTexture(*data.motion_vectors);
				GfxTexture& vrs_image_texture = ctx.GetTexture(*data.vrs_image);

				FfxVrsDispatchDescription vrs_dispatch_desc{};
				vrs_dispatch_desc.commandList = ffxGetCommandListDX12(cmd_list->GetNative());
				vrs_dispatch_desc.output = GetFfxResource(vrs_image_texture, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
				vrs_dispatch_desc.historyColor = GetFfxResource(color_history_texture);
				vrs_dispatch_desc.motionVectors = GetFfxResource(motion_vectors_texture);
				vrs_dispatch_desc.motionFactor = vrs_motion_factor;
				vrs_dispatch_desc.varianceCutoff = vrs_threshold;
				vrs_dispatch_desc.tileSize = shading_rate_image_tile_size;
				vrs_dispatch_desc.renderSize = { width, height };
				vrs_dispatch_desc.motionVectorScale.x = (float)width;
				vrs_dispatch_desc.motionVectorScale.y = (float)height;
				FfxErrorCode error_code = ffxVrsContextDispatch(&vrs_context, &vrs_dispatch_desc);
				ADRIA_ASSERT(error_code == FFX_OK);

				cmd_list->ResetState();
			}, RGPassType::Compute);
	}

	void FFXVRSPass::GUI()
	{

	}

	void FFXVRSPass::OnResize(uint32 w, uint32 h)
	{

	}

	void FFXVRSPass::DestroyContext()
	{
		if (context_created)
		{
			gfx->WaitForGPU();
			FfxErrorCode result = ffxVrsContextDestroy(&vrs_context);
			ADRIA_ASSERT(result == FFX_OK);
			context_created = false;
		}
	}

	void FFXVRSPass::CreateContext()
	{
		if (!context_created)
		{
			if (additional_shading_rates_supported)
			{
				vrs_context_description.flags |= FFX_VRS_ALLOW_ADDITIONAL_SHADING_RATES;
			}
			vrs_context_description.shadingRateImageTileSize = shading_rate_image_tile_size;
			FfxErrorCode result = ffxVrsContextCreate(&vrs_context, &vrs_context_description);
			ADRIA_ASSERT(result == FFX_OK);

			context_created = true;
		}
	}

}

