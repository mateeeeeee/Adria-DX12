#include "FFXVRSPass.h"
#include "FidelityFXUtils.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{

	FFXVRSPass::FFXVRSPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), ffx_interface(nullptr), vrs_context{}, vrs_context_description{}
	{
		if (!gfx->GetCapabilities().SupportsVSR())
		{
			return;
		}
		FillFeatureInfo();
		CreateVRSImage();

		sprintf(name_version, "FFX VRS %d.%d.%d", FFX_VRS_VERSION_MAJOR, FFX_VRS_VERSION_MINOR, FFX_VRS_VERSION_PATCH);
		ffx_interface = CreateFfxInterface(gfx, FFX_VRS_CONTEXT_COUNT);
		vrs_context_description.backendInterface = *ffx_interface;
		ADRIA_ASSERT(vrs_context_description.backendInterface.fpGetSDKVersion(&vrs_context_description.backendInterface) == FFX_SDK_MAKE_VERSION(1, 1, 0));
		ADRIA_ASSERT(ffxVrsGetEffectVersion() == FFX_SDK_MAKE_VERSION(1, 2, 0));

		CreateContext();
	}

	FFXVRSPass::~FFXVRSPass()
	{
		DestroyContext();
		DestroyFfxInterface(ffx_interface);
	}

	void FFXVRSPass::AddPass(RenderGraph& rendergraph)
	{

	}

	void FFXVRSPass::GUI()
	{

	}

	void FFXVRSPass::OnResize(uint32 w, uint32 h)
	{

	}

	void FFXVRSPass::FillFeatureInfo()
	{
		uint32& number_of_shading_rates = vrs_feature_info.number_of_shading_rates;

		vrs_feature_info.shading_rates[number_of_shading_rates++] = GfxShadingRate_1X1;
		vrs_feature_info.shading_rates[number_of_shading_rates++] = GfxShadingRate_1X2;
		vrs_feature_info.shading_rates[number_of_shading_rates++] = GfxShadingRate_2X1;
		vrs_feature_info.shading_rates[number_of_shading_rates++] = GfxShadingRate_2X2;

		vrs_feature_info.combiners = GfxShadingRateCombiner::Passthrough |
											  GfxShadingRateCombiner::Override | GfxShadingRateCombiner::Min |
											  GfxShadingRateCombiner::Max | GfxShadingRateCombiner::Sum;

		vrs_feature_info.additional_shading_rates_supported = gfx->GetCapabilities().SupportsAdditionalShadingRates();
		if (vrs_feature_info.additional_shading_rates_supported)
		{
			vrs_feature_info.shading_rates[number_of_shading_rates++] = GfxShadingRate_2X4;
			vrs_feature_info.shading_rates[number_of_shading_rates++] = GfxShadingRate_4X2;
			vrs_feature_info.shading_rates[number_of_shading_rates++] = GfxShadingRate_4X4;
		}

		if (gfx->GetCapabilities().CheckVSRSupport(VSRSupport::Tier2))
		{
			vrs_feature_info.shading_rate_image_tile_size = gfx->GetCapabilities().GetShadingRateImageTileSize();
			vrs_feature_info.shading_rate_image_tile_size = gfx->GetCapabilities().GetShadingRateImageTileSize();
			vrs_feature_info.shading_rate_image_tile_size = gfx->GetCapabilities().GetShadingRateImageTileSize();
			vrs_feature_info.shading_rate_image_tile_size = gfx->GetCapabilities().GetShadingRateImageTileSize();
		}
	}

	void FFXVRSPass::CreateVRSImage()
	{
		uint32 vrs_image_width, vrs_image_height;
		ffxVrsGetImageSizeFromeRenderResolution(&vrs_image_width, &vrs_image_height, width, height, vrs_feature_info.shading_rate_image_tile_size);
		GfxTextureDesc vrsImageDesc{};
		vrsImageDesc.format = GfxFormat::R8_UINT;
		vrsImageDesc.width = vrs_image_width;
		vrsImageDesc.height = vrs_image_height;
		vrsImageDesc.bind_flags = GfxBindFlag::UnorderedAccess;

		vrs_image = gfx->CreateTexture(vrsImageDesc);
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
			if (vrs_feature_info.additional_shading_rates_supported)
			{
				vrs_context_description.flags |= FFX_VRS_ALLOW_ADDITIONAL_SHADING_RATES;
			}
			vrs_context_description.shadingRateImageTileSize = vrs_feature_info.shading_rate_image_tile_size;
			FfxErrorCode result = ffxVrsContextCreate(&vrs_context, &vrs_context_description);
			ADRIA_ASSERT(result == FFX_OK);

			context_created = true;
		}
	}

}

