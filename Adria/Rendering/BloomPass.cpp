#include "BloomPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
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
			RGTextureReadWriteId extract;
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
				data.extract = builder.WriteTexture(RG_RES_NAME(BloomExtract));
				data.input = builder.ReadTexture(last_resource, ReadAccess_NonPixelShader);
			},
			[=](BloomExtractPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::BloomExtract));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::BloomExtract));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = context.GetReadOnlyTexture(data.input);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				++descriptor_index;
				cpu_descriptor = context.GetReadWriteTexture(data.extract);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->Dispatch((uint32)std::ceil(width / 32.0f),
					(uint32)std::ceil(height / 32.0f), 1);

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
			[=](BloomCombinePassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::BloomCombine));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::BloomCombine));

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = context.GetReadOnlyTexture(data.input);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cpu_descriptor = context.GetReadOnlyTexture(data.extract);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

				descriptor_index += 2;
				cpu_descriptor = context.GetReadWriteTexture(data.output);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->Dispatch((uint32)std::ceil(width / 32.0f), (uint32)std::ceil(height / 32.0f), 1);
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

