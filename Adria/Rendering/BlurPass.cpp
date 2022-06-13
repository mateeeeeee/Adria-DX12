#include "BlurPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
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
				TextureDesc blur_desc{};
				blur_desc.width = width;
				blur_desc.height = height;
				blur_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
				blur_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				blur_desc.initial_state = EResourceState::UnorderedAccess;

				builder.DeclareTexture(RG_RES_NAME_IDX(Intermediate, counter), blur_desc);
				data.dst_texture = builder.WriteTexture(RG_RES_NAME_IDX(Intermediate, counter));
				data.src_texture = builder.ReadTexture(src_texture, ReadAccess_NonPixelShader);

				builder.SetViewport(width, height);
			},
			[=](BlurPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Blur));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Blur_Horizontal));

				cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = context.GetReadOnlyTexture(data.src_texture);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				++descriptor_index;
				cpu_descriptor = context.GetReadWriteTexture(data.dst_texture);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->Dispatch((uint32)std::ceil(width / 1024.0f), height, 1);

				//vertical pass

			}, ERGPassType::Compute, ERGPassFlags::None);

		name = "Vertical Blur Pass" + std::string(pass_name);
		rendergraph.AddPass<BlurPassData>(name.c_str(),
			[=](BlurPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc blur_desc{};
				blur_desc.width = width;
				blur_desc.height = height;
				blur_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
				blur_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				blur_desc.initial_state = EResourceState::UnorderedAccess;

				builder.DeclareTexture(blurred_texture, blur_desc);
				data.dst_texture = builder.WriteTexture(blurred_texture);
				data.src_texture = builder.ReadTexture(RG_RES_NAME_IDX(Intermediate, counter), ReadAccess_NonPixelShader);

				builder.SetViewport(width, height);
			},
			[=](BlurPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Blur_Vertical));
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

				RGDescriptor cpu_descriptor = context.GetReadOnlyTexture(data.src_texture);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				++descriptor_index;
				cpu_descriptor = context.GetReadWriteTexture(data.dst_texture);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->Dispatch(width, (UINT)std::ceil(height / 1024.0f), 1);

			}, ERGPassType::Compute, ERGPassFlags::None);

	}

}