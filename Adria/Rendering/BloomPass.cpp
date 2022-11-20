#include "BloomPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"

namespace adria
{
	BloomPass::BloomPass(uint32 w, uint32 h) : width(w), height(h), generate_mips_pass(w, h)
	{}

	RGResourceName BloomPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		RGResourceName last_resource = input;
		struct BloomExtractPassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId input;
		};

		rg.AddPass<BloomExtractPassData>("BloomExtract Pass",
			[=](BloomExtractPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc bloom_extract_desc{};
				bloom_extract_desc.width = width;
				bloom_extract_desc.height = height;
				bloom_extract_desc.mip_levels = 5;
				bloom_extract_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(BloomExtract), bloom_extract_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(BloomExtract));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
			},
			[=](BloomExtractPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct BloomExtractConstants
				{
					uint32 input_idx;
					uint32 output_idx;

					float threshold;
					float scale;
				} constants =
				{
					.input_idx = i, .output_idx = i + 1,
					.threshold = params.bloom_threshold, .scale = params.bloom_scale
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::BloomExtract));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 4, &constants, 0);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		generate_mips_pass.AddPass(rg, RG_RES_NAME(BloomExtract));

		struct BloomCombinePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId  input;
			RGTextureReadOnlyId  extract;
		};
		rg.AddPass<BloomCombinePassData>("BloomCombine Pass",
			[=](BloomCombinePassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc bloom_output_desc{};
				bloom_output_desc.width = width;
				bloom_output_desc.height = height;
				bloom_output_desc.format = EFormat::R16G16B16A16_FLOAT;

				builder.DeclareTexture(RG_RES_NAME(BloomOutput), bloom_output_desc);
				data.output = builder.WriteTexture(RG_RES_NAME(BloomOutput));
				data.extract = builder.ReadTexture(RG_RES_NAME(BloomExtract), ReadAccess_NonPixelShader);
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
			},
			[=](BloomCombinePassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(3);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.input), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyTexture(data.extract), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct BloomCombineConstants
				{
					uint32 input_idx;
					uint32 output_idx;
					uint32 bloom_idx;
				} constants =
				{
					.input_idx = i, .output_idx = i + 1, .bloom_idx = i + 2
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::BloomCombine));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 3, &constants, 0);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		AddGUI([&]() 
			{
				if (ImGui::TreeNodeEx("Bloom", 0))
				{
					ImGui::SliderFloat("Threshold", &params.bloom_threshold, 0.1f, 2.0f);
					ImGui::SliderFloat("Bloom Scale", &params.bloom_scale, 0.1f, 5.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
		return RG_RES_NAME(BloomOutput);
	}

	void BloomPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		generate_mips_pass.OnResize(w, h);
	}
}

