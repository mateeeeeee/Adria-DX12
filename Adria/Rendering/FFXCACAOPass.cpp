#include "FFXCACAOPass.h"
#include "FidelityFXUtils.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{
	namespace
	{
		struct CacaoPreset 
		{
			bool use_downsampled_ssao;
			FfxCacaoSettings cacao_settings;
		};

		static std::vector<std::string> FfxCacaoPresetNames = 
		{
			"Native - Adaptive Quality",
			"Native - High Quality",
			"Native - Medium Quality",
			"Native - Low Quality",
			"Native - Lowest Quality",
			"Downsampled - Adaptive Quality",
			"Downsampled - High Quality",
			"Downsampled - Medium Quality",
			"Downsampled - Low Quality",
			"Downsampled - Lowest Quality",
			"Custom"
		};

		static const CacaoPreset FfxCacaoPresets[] = 
		{
			// Native - Adaptive Quality
			{
				/* useDownsampledSsao */ false,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGHEST,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 2,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.1f,
			}
		},
			// Native - High Quality
			{
				/* useDownsampledSsao */ false,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGH,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 2,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.1f,
			}
		},
			// Native - Medium Quality
			{
				/* useDownsampledSsao */ false,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_MEDIUM,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 2,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.1f,
			}
		},
			// Native - Low Quality
			{
				/* useDownsampledSsao */ false,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_LOW,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 6,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.1f,
			}
		},
			// Native - Lowest Quality
			{
				/* useDownsampledSsao */ false,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_LOWEST,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 6,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.1f,
			}
		},
			// Downsampled - Highest Quality
			{
				/* useDownsampledSsao */ true,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGHEST,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 2,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.1f,
			}
		},
			// Downsampled - High Quality
			{
				/* useDownsampledSsao */ true,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_HIGH,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 2,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.1f,
			}
		},
			// Downsampled - Medium Quality
			{
				/* useDownsampledSsao */ true,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_MEDIUM,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 3,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 5.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.2f,
			}
		},
			// Downsampled - Low Quality
			{
				/* useDownsampledSsao */ true,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_LOW,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 6,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 8.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.8f,
			}
		},
			// Downsampled - Lowest Quality
			{
				/* useDownsampledSsao */ true,
				{
				/* radius                            */ 1.2f,
				/* shadowMultiplier                  */ 1.0f,
				/* shadowPower                       */ 1.50f,
				/* shadowClamp                       */ 0.98f,
				/* horizonAngleThreshold             */ 0.06f,
				/* fadeOutFrom                       */ 20.0f,
				/* fadeOutTo                         */ 40.0f,
				/* qualityLevel                      */ FFX_CACAO_QUALITY_LOWEST,
				/* adaptiveQualityLimit              */ 0.75f,
				/* blurPassCount                     */ 6,
				/* sharpness                         */ 0.98f,
				/* temporalSupersamplingAngleOffset  */ 0.0f,
				/* temporalSupersamplingRadiusOffset */ 0.0f,
				/* detailShadowStrength              */ 0.5f,
				/* generateNormals                   */ false,
				/* bilateralSigmaSquared             */ 8.0f,
				/* bilateralSimilarityDistanceSigma  */ 0.8f,
			}
		}
		};
	}

	FFXCACAOPass::FFXCACAOPass(GfxDevice* gfx, FfxInterface& ffx_interface, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		if (!gfx->GetCapabilities().SupportsShaderModel(SM_6_6)) return;
		sprintf(name_version, "FFX CACAO %d.%d.%d", FFX_CACAO_VERSION_MAJOR, FFX_CACAO_VERSION_MINOR, FFX_CACAO_VERSION_PATCH);
		cacao_context_desc.backendInterface = ffx_interface;
		CreateContext();
	}

	FFXCACAOPass::~FFXCACAOPass()
	{
		DestroyContext();
	}

	RGResourceName FFXCACAOPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		struct FFXCACAOPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.AddPass<FFXCACAOPassData>(name_version,
			[=](FFXCACAOPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc ffx_cacao_output = builder.GetTextureDesc(input);
				builder.DeclareTexture(RG_RES_NAME(FFXCACAOOutput), ffx_cacao_output);

				data.output = builder.WriteTexture(RG_RES_NAME(FFXCACAOOutput));
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);
			},
			[=](FFXCACAOPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture& input_texture = ctx.GetTexture(*data.input);
				GfxTexture& output_texture = ctx.GetTexture(*data.output);

				FfxCacaoDispatchDescription cacao_dispatch_desc{};
				cacao_dispatch_desc.commandList = ffxGetCommandListDX12(cmd_list->GetNative());
				//todo

				FfxErrorCode errorCode = ffxCacaoContextDispatch(&cacao_context, &cacao_dispatch_desc);
				ADRIA_ASSERT(errorCode == FFX_OK);

				cmd_list->ResetState();
			}, RGPassType::Compute);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx(name_version, ImGuiTreeNodeFlags_None))
				{
					//todo
					ImGui::TreePop();
				}
			});

		return RG_RES_NAME(FFXCASOutput);
	}

	void FFXCACAOPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		DestroyContext();
		CreateContext();
	}

	void FFXCACAOPass::CreateContext()
	{
		cacao_context_desc.width = width;
		cacao_context_desc.height = height;
		cacao_context_desc.useDownsampledSsao = false;
		FfxErrorCode error_code = ffxCacaoContextCreate(&cacao_context, &cacao_context_desc);
		ADRIA_ASSERT(error_code == FFX_OK);
		cacao_context_desc.useDownsampledSsao = true;
		error_code = ffxCacaoContextCreate(&cacao_downsampled_context, &cacao_context_desc);
		ADRIA_ASSERT(error_code == FFX_OK);
	}

	void FFXCACAOPass::DestroyContext()
	{
		gfx->WaitForGPU();
		ffxCacaoContextDestroy(&cacao_context);
		ffxCacaoContextDestroy(&cacao_downsampled_context);
	}

}

