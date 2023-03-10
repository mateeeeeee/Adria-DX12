#include "MotionVectorsPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

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
				velocity_buffer_desc.format = EFormat::R16G16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(VelocityBuffer), velocity_buffer_desc);
				data.velocity = builder.WriteTexture(RG_RES_NAME(VelocityBuffer));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](MotionVectorsPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::MotionVectors));

				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.velocity), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct MotionVectorsConstants
				{
					uint32   depth_idx;
					uint32   output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};
				
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 2, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

	void MotionVectorsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}
}

