#include "ClusteredLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"
#include "../tecs/registry.h"
#include "../Logging/Logger.h"

using namespace DirectX;

namespace adria
{

	ClusteredLightingPass::ClusteredLightingPass(tecs::registry& reg, GraphicsDevice* gfx, uint32 w, uint32 h) : reg(reg), width(w), height(h),
		clusters(gfx, StructuredBufferDesc<ClusterAABB>(CLUSTER_COUNT)),
		light_counter(gfx, StructuredBufferDesc<uint32>(1)),
		light_list(gfx, StructuredBufferDesc<uint32>(CLUSTER_COUNT * CLUSTER_MAX_LIGHTS)),
		light_grid(gfx, StructuredBufferDesc<LightGrid>(CLUSTER_COUNT))
	{
		light_counter.CreateSRV();
		light_list.CreateSRV();
		clusters.CreateSRV();
		light_grid.CreateSRV();

		light_counter.CreateUAV();
		light_list.CreateUAV();
		clusters.CreateUAV();
		light_grid.CreateUAV();
	}

	void ClusteredLightingPass::AddPass(RenderGraph& rendergraph, bool recreate_clusters)
	{
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct ClusterBuildingPassData
		{
		};

		if (recreate_clusters)
		{
			rendergraph.AddPass<ClusterBuildingPassData>("Cluster Building Pass",
				[=](ClusterBuildingPassData& data, RenderGraphBuilder& builder)
				{

				},
				[=](ClusterBuildingPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
				{
					ID3D12Device* device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

					cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ClusterBuilding));
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ClusterBuilding));
					cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);

					OffsetType descriptor_index = descriptor_allocator->Allocate();
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, clusters.UAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, dst_descriptor);

					cmd_list->Dispatch(CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
				}, ERGPassType::Compute, ERGPassFlags::None);
		}

		auto light_view = reg.view<Light>();
		std::vector<StructuredLight> structured_lights{};
		for (auto e : light_view)
		{
			StructuredLight structured_light{};
			auto& light = light_view.get(e);
			structured_light.color = light.color * light.energy;
			structured_light.position = XMVector4Transform(light.position, global_data.camera_view);
			structured_light.direction = XMVector4Transform(light.direction, global_data.camera_view);
			structured_light.range = light.range;
			structured_light.type = static_cast<int>(light.type);
			structured_light.inner_cosine = light.inner_cosine;
			structured_light.outer_cosine = light.outer_cosine;
			structured_light.active = light.active;
			structured_light.casts_shadows = light.casts_shadows;
			structured_lights.push_back(structured_light);
		}

		struct ClusterCullingPassData
		{
			RGAllocationId alloc_id;
		};
		rendergraph.AddPass<ClusterCullingPassData>("Cluster Culling Pass",
			[=](ClusterCullingPassData& data, RenderGraphBuilder& builder)
			{
				uint64 alloc_size = sizeof(StructuredLight) * structured_lights.size();
				builder.DeclareAllocation(RG_RES_NAME(ClusterAllocation), AllocDesc{.size_in_bytes = alloc_size, .alignment = sizeof(StructuredLight) });
				data.alloc_id = builder.UseAllocation(RG_RES_NAME(ClusterAllocation));
			},
			[=](ClusterCullingPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto upload_buffer = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				DynamicAllocation& dynamic_alloc = resources.GetAllocation(data.alloc_id);
				dynamic_alloc.Update(structured_lights.data(), dynamic_alloc.size);

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ClusterCulling));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ClusterCulling));

				OffsetType i = descriptor_allocator->AllocateRange(2);
				auto dst_descriptor = descriptor_allocator->GetHandle(i);
				device->CopyDescriptorsSimple(1, dst_descriptor, clusters.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
				desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				desc.Format = DXGI_FORMAT_UNKNOWN;
				desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				desc.Buffer.StructureByteStride = sizeof(StructuredLight);
				desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
				desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;
				device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, descriptor_allocator->GetHandle(i + 1));

				cmd_list->SetComputeRootDescriptorTable(0, dst_descriptor);

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { light_counter.UAV(), light_list.UAV(), light_grid.UAV() };
				uint32 src_range_sizes[] = { 1,1,1 };
				i = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				dst_descriptor = descriptor_allocator->GetHandle(i);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetComputeRootDescriptorTable(1, dst_descriptor);
				cmd_list->Dispatch(CLUSTER_SIZE_X / 16, CLUSTER_SIZE_Y / 16, CLUSTER_SIZE_Z / 1);

			}, ERGPassType::Compute, ERGPassFlags::None);

		struct ClusterLightingPassData
		{
			RGAllocationId alloc_id;
			RGTextureReadOnlyId gbuffer_normal;
			RGTextureReadOnlyId gbuffer_albedo;
			RGTextureReadOnlyId depth;
		};
		rendergraph.AddPass<ClusterLightingPassData>("Cluster Lighting Pass",
			[=](ClusterLightingPassData& data, RenderGraphBuilder& builder)
			{
				data.alloc_id = builder.UseAllocation(RG_RES_NAME(ClusterAllocation));
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](ClusterLightingPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto upload_buffer = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ClusteredLightingPBR));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ClusteredLightingPBR));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);

				//gbuffer
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { resources.GetDescriptor(data.gbuffer_normal), 
															  resources.GetDescriptor(data.gbuffer_albedo), 
															  resources.GetDescriptor(data.depth) };
				uint32 src_range_sizes[] = { 1,1,1 };
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, dst_descriptor);

				//light stuff
				descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles) + 1);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { light_list.SRV(), light_grid.SRV() };
				uint32 src_range_sizes2[] = { 1,1 };

				dst_descriptor = descriptor_allocator->GetHandle(descriptor_index + 1);
				uint32 dst_range_sizes2[] = { (uint32)ARRAYSIZE(cpu_handles2) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes2, ARRAYSIZE(cpu_handles2), cpu_handles2, src_range_sizes2,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);

				DynamicAllocation& dynamic_alloc = resources.GetAllocation(data.alloc_id);
				D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
				desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				desc.Format = DXGI_FORMAT_UNKNOWN;
				desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				desc.Buffer.StructureByteStride = sizeof(StructuredLight);
				desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
				desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;
				device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, dst_descriptor);

				cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

}



