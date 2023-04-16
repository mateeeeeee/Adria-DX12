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
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
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
			[=](MotionVectorsPassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				auto descriptor_allocator = gfx->GetDescriptorAllocator();
				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::MotionVectors));

				uint32 i = descriptor_allocator->Allocate(2).GetIndex();
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth));
				gfx->CopyDescriptors(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.velocity));

				struct MotionVectorsConstants
				{
					uint32   depth_idx;
					uint32   output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};
				
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void MotionVectorsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}
}

