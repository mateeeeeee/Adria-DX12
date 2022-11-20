#include "BlurPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"

#include "../RenderGraph/RenderGraph.h"


namespace adria
{
	void BlurPass::AddPass(RenderGraph& rendergraph, RGResourceName src_texture, RGResourceName blurred_texture,
		char const* pass_name)
	{
		struct BlurPassData
		{
			RGTextureReadOnlyId src_texture;
			RGTextureReadWriteId dst_texture;
		};

		static uint64 counter = 0;
		counter++;

		std::string name = "Horizontal Blur Pass" + std::string(pass_name);
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		rendergraph.AddPass<BlurPassData>(name.c_str(),
			[=](BlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc blur_desc{};
				blur_desc.width = width;
				blur_desc.height = height;
				blur_desc.format = EFormat::R16G16B16A16_FLOAT;
				
				builder.DeclareTexture(RG_RES_NAME_IDX(Intermediate, counter), blur_desc);
				data.dst_texture = builder.WriteTexture(RG_RES_NAME_IDX(Intermediate, counter));
				data.src_texture = builder.ReadTexture(src_texture, ReadAccess_NonPixelShader);

				builder.SetViewport(width, height);
			},
			[=](BlurPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyTexture(data.src_texture),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), context.GetReadWriteTexture(data.dst_texture),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct BlurConstants
				{
					uint32 input_idx;
					uint32 output_idx;
				} constants = 
				{
					.input_idx = i, .output_idx = i + 1
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Blur_Horizontal));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 2, &constants, 0);
				cmd_list->Dispatch((uint32)std::ceil(width / 1024.0f), height, 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		name = "Vertical Blur Pass" + std::string(pass_name);
		rendergraph.AddPass<BlurPassData>(name.c_str(),
			[=](BlurPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc blur_desc{};
				blur_desc.width = width;
				blur_desc.height = height;
				blur_desc.format = EFormat::R16G16B16A16_FLOAT;
				
				builder.DeclareTexture(blurred_texture, blur_desc);
				data.dst_texture = builder.WriteTexture(blurred_texture);
				data.src_texture = builder.ReadTexture(RG_RES_NAME_IDX(Intermediate, counter), ReadAccess_NonPixelShader);
			},
			[=](BlurPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyTexture(data.src_texture),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), context.GetReadWriteTexture(data.dst_texture),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct BlurConstants
				{
					uint32 input_idx;
					uint32 output_idx;
				} constants =
				{
					.input_idx = i, .output_idx = i + 1
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Blur_Vertical));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 2, &constants, 0);
				cmd_list->Dispatch(width, (uint32)std::ceil(height / 1024.0f), 1);

			}, ERGPassType::Compute, ERGPassFlags::None);

	}

}