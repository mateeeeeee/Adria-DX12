#include "FSR2Pass.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "FSR2/dx12/ffx_fsr2_dx12.h"
#include "Editor/GUICommand.h"
#include "Logging/Logger.h"

namespace adria
{
	FSR2Pass::FSR2Pass(GfxDevice* _gfx, uint32 w, uint32 h)
		: gfx(_gfx), display_width(), display_height(), render_width(), render_height()
	{
		OnResize(w, h);
	}

	FSR2Pass::~FSR2Pass()
	{
		DestroyContext();
	}

	RGResourceName FSR2Pass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		if (recreate_context) CreateContext();

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct FSR2PassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId velocity;
			RGTextureReadOnlyId exposure;
			RGTextureReadWriteId output;
		};

		rg.AddPass<FSR2PassData>("FSR2 Pass",
			[=](FSR2PassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc fsr2_desc{};
				fsr2_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				fsr2_desc.width = display_width;
				fsr2_desc.height = display_height;
				fsr2_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(FSR2Output), fsr2_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(FSR2Output));
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);
				data.velocity = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				//data.exposure = builder.ReadTexture(RG_RES_NAME(Exposure), ReadAccess_NonPixelShader);
			},
			[=](FSR2PassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				if (recreate_context)
				{
					DestroyContext();
					CreateContext();
				}

				GfxTexture& input_texture = ctx.GetTexture(*data.input);
				GfxTexture& velocity_texture = ctx.GetTexture(*data.velocity);
				GfxTexture& depth_texture = ctx.GetTexture(*data.depth);
				GfxTexture& output_texture = ctx.GetTexture(*data.output);

				FfxFsr2DispatchDescription dispatch_desc{};
				dispatch_desc.commandList = ffxGetCommandListDX12(cmd_list->GetNative());
				dispatch_desc.color = ffxGetResourceDX12(&context, input_texture.GetNative());
				dispatch_desc.depth = ffxGetResourceDX12(&context, depth_texture.GetNative());
				dispatch_desc.motionVectors = ffxGetResourceDX12(&context, velocity_texture.GetNative());
				dispatch_desc.exposure = ffxGetResourceDX12(&context, nullptr);
				dispatch_desc.reactive = ffxGetResourceDX12(&context, nullptr); 
				dispatch_desc.transparencyAndComposition = ffxGetResourceDX12(&context, nullptr);
				dispatch_desc.output = ffxGetResourceDX12(&context, output_texture.GetNative(), L"FSR2 Output", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
				dispatch_desc.jitterOffset.x = frame_data.camera_jitter_x;
				dispatch_desc.jitterOffset.y = frame_data.camera_jitter_y;
				dispatch_desc.motionVectorScale.x = (float)render_width;
				dispatch_desc.motionVectorScale.y = (float)render_height;
				dispatch_desc.reset = false;
				dispatch_desc.enableSharpening = true;
				dispatch_desc.sharpness = sharpness;
				dispatch_desc.frameTimeDelta = frame_data.frame_delta_time;
				dispatch_desc.preExposure = 1.0f;
				dispatch_desc.renderSize.width = render_width;
				dispatch_desc.renderSize.height = render_height;
				dispatch_desc.cameraFar = frame_data.camera_far;
				dispatch_desc.cameraNear = frame_data.camera_near;
				dispatch_desc.cameraFovAngleVertical = frame_data.camera_fov;

				FfxErrorCode error_code = ffxFsr2ContextDispatch(&context, &dispatch_desc);
				ADRIA_ASSERT(error_code == FFX_OK);

				cmd_list->ResetState();
			}, RGPassType::Compute);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx("FSR2", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					if (ImGui::Combo("Quality Mode", (int32*)&quality_mode, "Custom\0Quality (1.5x)\0Balanced (1.7x)\0Performance (2.0x)\0Ultra Performance (3.0x)\0", 5))
					{
						RecreateRenderDimensions();
						recreate_context = true;
					}

					if (quality_mode == 0)
					{
						if (ImGui::SliderFloat("Upscale Ratio", &custom_upscale_ratio, 1.0, 3.0))
						{
							RecreateRenderDimensions();
							recreate_context = true;
						}
					}
					
					ImGui::SliderFloat("Sharpness", &sharpness, 0.0f, 1.0f, "%.2f");
					ImGui::TreePop(); 
				}
			});

		return RG_RES_NAME(FSR2Output);
	}

	float FSR2Pass::GetUpscaleRatio() const
	{
		return quality_mode == 0 ? custom_upscale_ratio : ffxFsr2GetUpscaleRatioFromQualityMode(quality_mode);
	}

	void FSR2Pass::CreateContext()
	{
		if (recreate_context)
		{
			DestroyContext();

			ID3D12Device* device = gfx->GetDevice();

			uint64 const scratch_buffer_size = ffxFsr2GetScratchMemorySizeDX12();
			void* scratch_buffer = malloc(scratch_buffer_size);
			FfxErrorCode error_code = ffxFsr2GetInterfaceDX12(&context_desc.callbacks, device, scratch_buffer, scratch_buffer_size);
			ADRIA_ASSERT(error_code == FFX_OK);

			context_desc.device = ffxGetDeviceDX12(device);
			context_desc.maxRenderSize.width = render_width;
			context_desc.maxRenderSize.height = render_height;
			context_desc.displaySize.width = display_width;
			context_desc.displaySize.height = display_height;
			context_desc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_AUTO_EXPOSURE;

			ffxFsr2ContextCreate(&context, &context_desc);
			recreate_context = false;
		}
	}

	void FSR2Pass::DestroyContext()
	{
		if (context_desc.callbacks.scratchBuffer)
		{
			gfx->WaitForGPU();
			ffxFsr2ContextDestroy(&context);
			free(context_desc.callbacks.scratchBuffer);
			context_desc.callbacks.scratchBuffer = nullptr;
		}
	}

	void FSR2Pass::RecreateRenderDimensions()
	{
		float upscale_ratio = GetUpscaleRatio();
		render_width = (uint32)((float)display_width / upscale_ratio);
		render_height = (uint32)((float)display_height / upscale_ratio);
		render_resolution_changed_event.Broadcast(render_width, render_height);
	}

}