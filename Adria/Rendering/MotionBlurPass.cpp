#include "MotionBlurPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	MotionBlurPass::MotionBlurPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h) 
	{
		CreatePSO();
	}

	RGResourceName MotionBlurPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		RGResourceName last_resource = input;
		struct MotionBlurPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId velocity;
			RGTextureReadWriteId output;
		};
		rg.AddPass<MotionBlurPassData>("Motion Blur Pass",
			[=](MotionBlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc motion_blur_desc{};
				motion_blur_desc.width = width;
				motion_blur_desc.height = height;
				motion_blur_desc.format = GfxFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(MotionBlurOutput), motion_blur_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(MotionBlurOutput));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
				data.velocity = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
			},
			[=](MotionBlurPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.input),
					ctx.GetReadOnlyTexture(data.velocity),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct MotionBlurConstants
				{
					uint32 scene_idx;
					uint32 velocity_idx;
					uint32 output_idx;
				} constants =
				{
					.scene_idx = i, .velocity_idx = i + 1, .output_idx = i + 2
				};

				cmd_list->SetPipelineState(motion_blur_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		return RG_RES_NAME(MotionBlurOutput);
	}

	void MotionBlurPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void MotionBlurPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_MotionBlur;
		motion_blur_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

