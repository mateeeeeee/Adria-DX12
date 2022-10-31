#include "TAAPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"

namespace adria
{

	TAAPass::TAAPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	RGResourceName TAAPass::AddPass(RenderGraph& rg, RGResourceName input, RGResourceName history)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = input;

		struct TAAPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId history;
			RGTextureReadOnlyId velocity;
			RGTextureReadWriteId output;
		};

		rg.AddPass<TAAPassData>("TAA Pass",
			[=](TAAPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc taa_desc{};
				taa_desc.width = width;
				taa_desc.height = height;
				taa_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(TAAOutput), taa_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(TAAOutput));
				data.input = builder.ReadTexture(last_resource, ReadAccess_PixelShader);
				data.history = builder.ReadTexture(RG_RES_NAME(HistoryBuffer), ReadAccess_PixelShader);
				data.velocity = builder.ReadTexture(RG_RES_NAME(VelocityBuffer), ReadAccess_PixelShader);
			},
			[=](TAAPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(4);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.history), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyTexture(data.velocity), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct TAAConstants
				{
					uint32 scene_idx;
					uint32 prev_scene_idx;
					uint32 velocity_idx;
					uint32 output_idx;
				} constants =
				{
					.scene_idx = i, .prev_scene_idx = i + 1, .velocity_idx = i + 2, .output_idx = i + 3
				};

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::TAA));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.new_frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 4, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		return RG_RES_NAME(TAAOutput);
	}

	void TAAPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}