#include "BlurPass.h"
#include "../ConstantBuffers.h"
#include "../Components.h"
#include "../GlobalBlackboardData.h"
#include "../RootSigPSOManager.h"
#include "../../RenderGraph/RenderGraph.h"

namespace adria
{

	BlurPassData const& BlurPass::AddPass(RenderGraph& rendergraph, RGTextureSRVRef texture_srv, char const* _name /*= ""*/)
	{
		std::string name = "Blur Pass" + std::string(_name);
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();
		return rendergraph.AddPass<BlurPassData>(name.c_str(),
			[=](BlurPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc blur_desc{};
				blur_desc.width = width;
				blur_desc.height = height;
				blur_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
				blur_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				blur_desc.initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

				RGTextureRef intermediate_texture = builder.CreateTexture("Intermediate Blur Texture", blur_desc);
				RGTextureRef final_texture = builder.CreateTexture("Intermediate Blur Texture", blur_desc);

				builder.Read(texture_srv.GetResourceHandle(), ReadAccess_NonPixelShader);
				builder.Write(final_texture, WriteAccess_Unordered);

				data.src_srv = texture_srv;
				data.intermediate_srv = builder.CreateSRV(intermediate_texture);
				data.intermediate_uav = builder.CreateUAV(intermediate_texture);
				data.final_srv = builder.CreateSRV(final_texture);
				data.final_uav = builder.CreateUAV(final_texture);
				builder.SetViewport(width, height);
			},
			[=](BlurPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Blur));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Blur_Horizontal));

				cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = resources.GetSRV(data.src_srv);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				++descriptor_index;
				cpu_descriptor = resources.GetUAV(data.intermediate_uav);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->Dispatch((uint32)std::ceil(width / 1024.0f), height, 1);

				ResourceBarrierBatch barriers{};
				Texture& intermediate_texture = resources.GetTexture(data.intermediate_srv.GetResourceHandle());
				barriers.AddTransition(intermediate_texture.GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				barriers.Submit(cmd_list);

				//vertical pass
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Blur_Vertical));
				descriptor_index = descriptor_allocator->AllocateRange(2);

				cpu_descriptor = resources.GetSRV(data.intermediate_srv);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				++descriptor_index;
				cpu_descriptor = resources.GetUAV(data.final_uav);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->Dispatch(width, (UINT)std::ceil(height / 1024.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

}

