#include "ClusteredDeferredLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "../Logging/Logger.h"

using namespace DirectX;

namespace adria
{

	ClusteredDeferredLightingPass::ClusteredDeferredLightingPass(entt::registry& reg, GraphicsDevice* gfx, uint32 w, uint32 h) : reg(reg), width(w), height(h),
		clusters(gfx, StructuredBufferDesc<ClusterAABB>(CLUSTER_COUNT)),
		light_counter(gfx, StructuredBufferDesc<uint32>(1)),
		light_list(gfx, StructuredBufferDesc<uint32>(CLUSTER_COUNT * CLUSTER_MAX_LIGHTS)),
		light_grid(gfx, StructuredBufferDesc<LightGrid>(CLUSTER_COUNT))
	{
	}

	void ClusteredDeferredLightingPass::AddPass(RenderGraph& rendergraph, bool recreate_clusters)
	{
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();

		rendergraph.ImportBuffer(RG_RES_NAME(ClustersBuffer), &clusters);
		rendergraph.ImportBuffer(RG_RES_NAME(LightCounter), &light_counter);
		rendergraph.ImportBuffer(RG_RES_NAME(LightGrid), &light_grid);
		rendergraph.ImportBuffer(RG_RES_NAME(LightList), &light_list);

		struct ClusterBuildingPassData
		{
			RGBufferReadWriteId cluster_uav;
		};

		if (recreate_clusters)
		{
			rendergraph.AddPass<ClusterBuildingPassData>("Cluster Building Pass",
				[=](ClusterBuildingPassData& data, RenderGraphBuilder& builder)
				{
					data.cluster_uav = builder.WriteBuffer(RG_RES_NAME(ClustersBuffer));
				},
				[=](ClusterBuildingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					ID3D12Device* device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

					cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::ClusterBuilding));
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ClusterBuilding));
					cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);

					OffsetType descriptor_index = descriptor_allocator->Allocate();
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, context.GetReadWriteBuffer(data.cluster_uav), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, dst_descriptor);

					cmd_list->Dispatch(CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
				}, ERGPassType::Compute, ERGPassFlags::None);
		}

		struct ClusterCullingPassData
		{
			RGAllocationId alloc_id;
			RGBufferReadOnlyId clusters_srv;
			RGBufferReadWriteId light_counter_uav;
			RGBufferReadWriteId light_grid_uav;
			RGBufferReadWriteId light_list_uav;
		};
		rendergraph.AddPass<ClusterCullingPassData>("Cluster Culling Pass",
			[=](ClusterCullingPassData& data, RenderGraphBuilder& builder)
			{
				data.clusters_srv = builder.ReadBuffer(RG_RES_NAME(ClustersBuffer), ReadAccess_NonPixelShader);
				data.light_counter_uav = builder.WriteBuffer(RG_RES_NAME(LightCounter));
				data.light_grid_uav = builder.WriteBuffer(RG_RES_NAME(LightGrid));
				data.light_list_uav = builder.WriteBuffer(RG_RES_NAME(LightList));
			},
			[=](ClusterCullingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto upload_buffer = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::ClusterCulling));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ClusterCulling));

				OffsetType i = descriptor_allocator->AllocateRange(2);
				auto dst_descriptor = descriptor_allocator->GetHandle(i);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyBuffer(data.clusters_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), global_data.lights_buffer_cpu_srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(0, dst_descriptor);

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = {	context.GetReadWriteBuffer(data.light_counter_uav),
																context.GetReadWriteBuffer(data.light_list_uav),
																context.GetReadWriteBuffer(data.light_grid_uav)};
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
			RGBufferReadOnlyId light_grid_srv;
			RGBufferReadOnlyId light_list_srv;
		};
		rendergraph.AddPass<ClusterLightingPassData>("Cluster Lighting Pass",
			[=](ClusterLightingPassData& data, RenderGraphBuilder& builder)
			{
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				data.light_grid_srv = builder.ReadBuffer(RG_RES_NAME(LightGrid), ReadAccess_PixelShader);
				data.light_list_srv = builder.ReadBuffer(RG_RES_NAME(LightList), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](ClusterLightingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto upload_buffer = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::ClusteredDeferredLighting));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ClusteredDeferredLighting));
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);

				//gbuffer
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal), 
															  context.GetReadOnlyTexture(data.gbuffer_albedo), 
															  context.GetReadOnlyTexture(data.depth) };
				uint32 src_range_sizes[] = { 1,1,1 };
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, dst_descriptor);

				//light stuff
				descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles) + 1);
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { context.GetReadOnlyBuffer(data.light_list_srv), context.GetReadOnlyBuffer(data.light_grid_srv) };
				uint32 src_range_sizes2[] = { 1,1 };

				dst_descriptor = descriptor_allocator->GetHandle(descriptor_index + 1);
				uint32 dst_range_sizes2[] = { (uint32)ARRAYSIZE(cpu_handles2) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes2, ARRAYSIZE(cpu_handles2), cpu_handles2, src_range_sizes2,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, global_data.lights_buffer_cpu_srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);
				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);

			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

}



