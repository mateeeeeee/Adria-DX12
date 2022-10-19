#include "FXAAPass.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	FXAAPass::FXAAPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void FXAAPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct FXAAPassData
		{
			RGTextureReadWriteId	output;
			RGTextureReadOnlyId		ldr;
		};

		rg.AddPass<FXAAPassData>("FXAA Pass",
			[=](FXAAPassData& data, RenderGraphBuilder& builder)
			{
				data.ldr = builder.ReadTexture(input, ReadAccess_NonPixelShader);
				ADRIA_ASSERT(builder.IsTextureDeclared(RG_RES_NAME(FinalTexture)));
				data.output = builder.WriteTexture(RG_RES_NAME(FinalTexture));
			},
			[=](FXAAPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.ldr), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				struct SSRConstants
				{
					uint32 depth_idx;
					uint32 output_idx;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1
				};

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::FXAA));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.new_frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 2, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16), (UINT)std::ceil(height / 16), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

	void FXAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}
