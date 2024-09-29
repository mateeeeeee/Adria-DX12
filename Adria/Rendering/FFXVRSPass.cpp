#include "FFXVRSPass.h"
#include "FidelityFXUtils.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Postprocessor.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"

namespace adria
{
	static TAutoConsoleVariable<bool>  VariableRateShading("r.VariableRateShading", false, "");
	static TAutoConsoleVariable<bool>  VariableRateShadingImage("r.VariableRateShading.Image", false, "");
	static TAutoConsoleVariable<bool>  VariableRateShadingOverlay("r.VariableRateShading.Overlay", false, "");
	static TAutoConsoleVariable<int>   VariableRateShadingMode("r.VariableRateShading.Mode", 0, "");
	static TAutoConsoleVariable<int>   VariableRateShadingCombiner("r.VariableRateShading.Combiner", 0, "");
	static TAutoConsoleVariable<float> VariableRateShadingThreshold("r.VariableRateShading.Threshold", 0.015f, "");
	static TAutoConsoleVariable<float> VariableRateShadingMotionFactor("r.VariableRateShading.MotionFactor", 0.01f, "");

	static GfxShadingRate shading_rates[] =
	{
		GfxShadingRate_1X1,
		GfxShadingRate_1X2,
		GfxShadingRate_2X1,
		GfxShadingRate_2X2,
		GfxShadingRate_2X4,
		GfxShadingRate_4X2,
		GfxShadingRate_4X4,
	};

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

		CreateVRSImage();
		CreateOverlayPSO();
		CreateContext();
	}

	FFXVRSPass::~FFXVRSPass()
	{
		DestroyContext();
		DestroyFfxInterface(ffx_interface);
	}

	void FFXVRSPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		if (!IsSupported()) return;

		GfxShadingRate shading_rate = shading_rates[VariableRateShadingMode.Get()];
		GfxShadingRateCombiner shading_rate_combiner = static_cast<GfxShadingRateCombiner>(VariableRateShadingCombiner.Get());

		if (!VariableRateShading.Get())
		{
			GfxShadingRateInfo info{};
			info.shading_mode = GfxVariableShadingMode::None;
			info.shading_rate = GfxShadingRate_1X1;
			info.shading_rate_combiner = GfxShadingRateCombiner::Passthrough;
			info.shading_rate_image = nullptr;
			gfx->SetVRSInfo(info);
			return;
		}

		if (!VariableRateShadingImage.Get())
		{
			GfxShadingRateInfo info{};
			info.shading_mode = GfxVariableShadingMode::PerDraw;
			info.shading_rate = shading_rate;
			info.shading_rate_combiner = shading_rate_combiner;
			info.shading_rate_image = nullptr;
			gfx->SetVRSInfo(info);
			return;
		}

		struct FFXVRSPassData
		{
			RGTextureReadOnlyId  color_history;
			RGTextureReadOnlyId  motion_vectors;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.AddPass<FFXVRSPassData>(name_version,
			[=](FFXVRSPassData& data, RenderGraphBuilder& builder)
			{
				data.color_history = builder.ReadTexture(RG_NAME(HistoryBuffer), ReadAccess_NonPixelShader);
				data.motion_vectors = builder.ReadTexture(RG_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
			},
			[=](FFXVRSPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture& color_history_texture = ctx.GetTexture(*data.color_history);
				GfxTexture& motion_vectors_texture = ctx.GetTexture(*data.motion_vectors);

				FfxVrsDispatchDescription vrs_dispatch_desc{};
				vrs_dispatch_desc.commandList = ffxGetCommandListDX12(cmd_list->GetNative());
				vrs_dispatch_desc.output = GetFfxResource(*vrs_image, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
				vrs_dispatch_desc.historyColor = GetFfxResource(color_history_texture);
				vrs_dispatch_desc.motionVectors = GetFfxResource(motion_vectors_texture);
				vrs_dispatch_desc.motionFactor = VariableRateShadingMotionFactor.Get();
				vrs_dispatch_desc.varianceCutoff = VariableRateShadingThreshold.Get();
				vrs_dispatch_desc.tileSize = shading_rate_image_tile_size;
				vrs_dispatch_desc.renderSize = { width, height };
				vrs_dispatch_desc.motionVectorScale.x = -1.0f;
				vrs_dispatch_desc.motionVectorScale.y = -1.0f;
				FfxErrorCode error_code = ffxVrsContextDispatch(&vrs_context, &vrs_dispatch_desc);
				ADRIA_ASSERT(error_code == FFX_OK);

				cmd_list->ResetState();

				GfxShadingRateInfo info{};
				info.shading_mode = GfxVariableShadingMode::Image;
				info.shading_rate = shading_rate;
				info.shading_rate_combiner = shading_rate_combiner;
				info.shading_rate_image = vrs_image.get();
				gfx->SetVRSInfo(info);

			}, RGPassType::Compute, RGPassFlags::ForceNoCull);

		if (VariableRateShadingOverlay.Get())
		{
			rg.AddPass<void>("VRS Overlay Pass",
				[=](RenderGraphBuilder& builder)
				{
					builder.WriteRenderTarget(postprocessor->GetFinalResource(), RGLoadStoreAccessOp::Preserve_Preserve);
					builder.SetViewport(width, height);
				},
				[=](RenderGraphContext& context, GfxCommandList* cmd_list)
				{
					GfxDescriptor dst = gfx->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst, vrs_image_srv);

					cmd_list->SetPipelineState(vrs_overlay_pso.get());
					cmd_list->TextureBarrier(*vrs_image, GfxResourceState::ComputeUAV, GfxResourceState::PixelSRV);
					cmd_list->SetRootConstant(1, dst.GetIndex(), 0);
					cmd_list->SetRootConstant(1, shading_rate_image_tile_size, 1);
					cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
					cmd_list->Draw(3);
					cmd_list->TextureBarrier(*vrs_image, GfxResourceState::PixelSRV, GfxResourceState::ComputeUAV);
				}, RGPassType::Graphics, RGPassFlags::None);
		}
	}

	void FFXVRSPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNode("Variable Rate Shading"))
				{
					ImGui::Checkbox("Enable", VariableRateShading.GetPtr());
					if (VariableRateShading.Get())
					{
						if (ImGui::Combo("Shading Rate", VariableRateShadingMode.GetPtr(), "1x1\0 1x2\0 2x1\0 2x2\0 2x4\0 4x2\0 4x4\0", 7))
						{
							//#todo verify support
						}
						ImGui::Combo("Shading Rate Combiner", VariableRateShadingCombiner.GetPtr(), "Passthrough\0 Override\0 Min\0 Max\0 Sum\0", 5);
						ImGui::Checkbox("Shading Rate Image", VariableRateShadingImage.GetPtr());

						if (VariableRateShadingImage.Get())
						{
							ImGui::SliderFloat("Threshold", VariableRateShadingThreshold.GetPtr(), 0.0f, 0.1f);
							ImGui::SliderFloat("Motion Factor", VariableRateShadingMotionFactor.GetPtr(), 0.0f, 0.1f);
							ImGui::Checkbox("Draw Overlay", VariableRateShadingOverlay.GetPtr());
						}
					}
					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_None);
	}

	void FFXVRSPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		CreateVRSImage();
	}

	bool FFXVRSPass::IsEnabled(PostProcessor const*) const
	{
		return true;
	}

	void FFXVRSPass::CreateVRSImage()
	{
		uint32 vrs_image_width, vrs_image_height;
		ffxVrsGetImageSizeFromeRenderResolution(&vrs_image_width, &vrs_image_height, width, height, shading_rate_image_tile_size);
		GfxTextureDesc vrs_image_desc{};
		vrs_image_desc.format = GfxFormat::R8_UINT;
		vrs_image_desc.width = vrs_image_width;
		vrs_image_desc.height = vrs_image_height;
		vrs_image_desc.bind_flags = GfxBindFlag::UnorderedAccess;
		vrs_image_desc.initial_state = GfxResourceState::ComputeUAV;
		vrs_image = gfx->CreateTexture(vrs_image_desc);
		vrs_image_srv = gfx->CreateTextureSRV(vrs_image.get());
	}

	void FFXVRSPass::CreateOverlayPSO()
	{
		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_FullscreenTriangle;
		gfx_pso_desc.PS = PS_VRSOverlay;
		gfx_pso_desc.num_render_targets = 1;
		gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Triangle;
		gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
		gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
		gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
		gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
		gfx_pso_desc.blend_state.render_target[0].blend_op_alpha = GfxBlendOp::Add;
		gfx_pso_desc.blend_state.render_target[0].src_blend_alpha = GfxBlend::One;
		gfx_pso_desc.blend_state.render_target[0].dest_blend_alpha = GfxBlend::InvSrcAlpha;
		vrs_overlay_pso = gfx->CreateGraphicsPipelineState(gfx_pso_desc);
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

