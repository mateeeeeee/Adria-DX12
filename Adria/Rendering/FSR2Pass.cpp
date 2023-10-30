#include "FSR2Pass.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "FSR2/dx12/ffx_fsr2_dx12.h"
#include "Editor/GUICommand.h"

namespace adria
{

	FSR2Pass::FSR2Pass(GfxDevice* _gfx) : gfx(_gfx)
	{

	}

	FSR2Pass::~FSR2Pass()
	{
		DestroyContext();
	}

	void FSR2Pass::AddPass(RenderGraph& rg, RGResourceName input)
	{

		struct FSR2PassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId velocity;
			RGTextureReadOnlyId exposure;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<FSR2PassData>("FSR2 Pass",
			[=](FSR2PassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc fsr2_desc{};
				fsr2_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				fsr2_desc.width = width;
				fsr2_desc.height = height;
				fsr2_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(FSR2Output), fsr2_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);
				data.velocity = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				//data.exposure = builder.ReadTexture(RG_RES_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
			},
			[=](FSR2PassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				if (need_create_context)
				{
					DestroyContext();
					CreateContext();
				}

				GfxTexture& input_texture = ctx.GetTexture(*data.input);
				GfxTexture& velocity_texture = ctx.GetTexture(*data.velocity);
				GfxTexture& depth_texture = ctx.GetTexture(*data.depth);
				GfxTexture& output_texture = ctx.GetTexture(*data.output);

				FfxFsr2DispatchDescription dispatchDesc = {};
				dispatchDesc.commandList = ffxGetCommandListDX12(cmd_list->GetNative());
				dispatchDesc.color = ffxGetResourceDX12(&context, input_texture.GetNative());
				dispatchDesc.depth = ffxGetResourceDX12(&context, depth_texture.GetNative());
				dispatchDesc.motionVectors = ffxGetResourceDX12(&context, velocity_texture.GetNative());
				dispatchDesc.exposure = ffxGetResourceDX12(&context, nullptr);
				dispatchDesc.reactive = ffxGetResourceDX12(&context, nullptr); 
				dispatchDesc.transparencyAndComposition = ffxGetResourceDX12(&context, nullptr);
				dispatchDesc.output = ffxGetResourceDX12(&context, output_texture.GetNative(), L"FSR2_Output", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
				dispatchDesc.jitterOffset.x = 0.0f; //todo
				dispatchDesc.jitterOffset.y = 0.0f; //todo
				dispatchDesc.motionVectorScale.x = -0.5f * (float)render_width;
				dispatchDesc.motionVectorScale.y = 0.5f * (float)render_height;
				dispatchDesc.reset = false;
				dispatchDesc.enableSharpening = true;
				dispatchDesc.sharpness = sharpness;
				dispatchDesc.frameTimeDelta = 0.0f; //todo
				dispatchDesc.preExposure = 1.0f;
				dispatchDesc.renderSize.width = render_width;
				dispatchDesc.renderSize.height = render_height;
				dispatchDesc.cameraFar = 0.0f; //todo
				dispatchDesc.cameraNear = 0.0f; //todo
				dispatchDesc.cameraFovAngleVertical = 0.0f; //todo

				FfxErrorCode errorCode = ffxFsr2ContextDispatch(&context, &dispatchDesc);
				ADRIA_ASSERT(errorCode == FFX_OK);
			}, RGPassType::Compute);

	}

	float FSR2Pass::GetUpscaleRatio() const
	{
		return quality_mode == 0 ? custom_upscale_ratio : ffxFsr2GetUpscaleRatioFromQualityMode(quality_mode);
	}

	void FSR2Pass::CreateContext()
	{
		if (need_create_context)
		{
			ID3D12Device* device = gfx->GetDevice();

			uint64 const scratch_buffer_size = ffxFsr2GetScratchMemorySizeDX12();
			void* scratch_buffer = malloc(scratch_buffer_size);
			FfxErrorCode errorCode = ffxFsr2GetInterfaceDX12(&context_desc.callbacks, device, scratch_buffer, scratch_buffer_size);
			ADRIA_ASSERT(errorCode == FFX_OK);

			float upscale_ratio = GetUpscaleRatio();
			render_width = (uint32)((float)width / upscale_ratio);
			render_height = (uint32)((float)width / upscale_ratio);

			context_desc.device = ffxGetDeviceDX12(device);
			context_desc.maxRenderSize.width = render_width;
			context_desc.maxRenderSize.height = render_height;
			context_desc.displaySize.width = width;
			context_desc.displaySize.height = height;
			context_desc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE;

			ffxFsr2ContextCreate(&context, &context_desc);
			need_create_context = false;
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

}