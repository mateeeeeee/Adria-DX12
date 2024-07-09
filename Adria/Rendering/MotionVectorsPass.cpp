#include "MotionVectorsPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"

namespace adria
{
	MotionVectorsPass::MotionVectorsPass(uint32 w, uint32 h)
		: width(w), height(h)
	{}

	void MotionVectorsPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct MotionVectorsPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId velocity;
		};
		rg.AddPass<MotionVectorsPassData>("Velocity Buffer Pass",
			[=](MotionVectorsPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc velocity_buffer_desc{};
				velocity_buffer_desc.width = width;
				velocity_buffer_desc.height = height;
				velocity_buffer_desc.format = GfxFormat::R16G16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(VelocityBuffer), velocity_buffer_desc);
				data.velocity = builder.WriteTexture(RG_RES_NAME(VelocityBuffer));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](MotionVectorsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadWriteTexture(data.velocity)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				struct MotionVectorsConstants
				{
					uint32   depth_idx;
					uint32   output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::MotionVectors));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void MotionVectorsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}
}

