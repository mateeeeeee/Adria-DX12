#include "ClusteredDeferredLightingPass.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "../Logging/Logger.h"

using namespace DirectX;

namespace adria
{
	struct ClusterAABB
	{
		DirectX::XMVECTOR min_point;
		DirectX::XMVECTOR max_point;
	};

	struct LightGrid
	{
		uint32 offset;
		uint32 light_count;
	};

	ClusteredDeferredLightingPass::ClusteredDeferredLightingPass(entt::registry& reg, GraphicsDevice* gfx, uint32 w, uint32 h) : reg(reg), width(w), height(h),
		clusters(gfx, StructuredBufferDesc<ClusterAABB>(CLUSTER_COUNT)),
		light_counter(gfx, StructuredBufferDesc<uint32>(1)),
		light_list(gfx, StructuredBufferDesc<uint32>(CLUSTER_COUNT * CLUSTER_MAX_LIGHTS)),
		light_grid(gfx, StructuredBufferDesc<LightGrid>(CLUSTER_COUNT))
	{
	}

	void ClusteredDeferredLightingPass::AddPass(RenderGraph& rendergraph, bool recreate_clusters)
	{
		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();

		rendergraph.ImportBuffer(RG_RES_NAME(ClustersBuffer), &clusters);
		rendergraph.ImportBuffer(RG_RES_NAME(LightCounter), &light_counter);
		rendergraph.ImportBuffer(RG_RES_NAME(LightGrid), &light_grid);
		rendergraph.ImportBuffer(RG_RES_NAME(LightList), &light_list);

		struct ClusterBuildingPassData
		{
			RGBufferReadWriteId clusters;
		};

		if (recreate_clusters)
		{
			rendergraph.AddPass<ClusterBuildingPassData>("Cluster Building Pass",
				[=](ClusterBuildingPassData& data, RenderGraphBuilder& builder)
				{
					data.clusters = builder.WriteBuffer(RG_RES_NAME(ClustersBuffer));
				},
				[=](ClusterBuildingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					ID3D12Device* device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

					OffsetType i = descriptor_allocator->Allocate();
					auto dst_descriptor = descriptor_allocator->GetHandle(i);
					device->CopyDescriptorsSimple(1, dst_descriptor, context.GetReadWriteBuffer(data.clusters), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ClusterBuilding));
					cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetComputeRoot32BitConstant(1, (uint32)i, 0);
					cmd_list->Dispatch(CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
				}, ERGPassType::Compute, ERGPassFlags::None);
		}

		struct ClusterCullingPassData
		{
			RGBufferReadOnlyId  clusters;
			RGBufferReadWriteId light_counter;
			RGBufferReadWriteId light_grid;
			RGBufferReadWriteId light_list;
		};
		rendergraph.AddPass<ClusterCullingPassData>("Cluster Culling Pass",
			[=](ClusterCullingPassData& data, RenderGraphBuilder& builder)
			{
				data.clusters = builder.ReadBuffer(RG_RES_NAME(ClustersBuffer), ReadAccess_NonPixelShader);
				data.light_counter = builder.WriteBuffer(RG_RES_NAME(LightCounter));
				data.light_grid = builder.WriteBuffer(RG_RES_NAME(LightGrid));
				data.light_list = builder.WriteBuffer(RG_RES_NAME(LightList));
			},
			[=](ClusterCullingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto upload_buffer = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = {	context.GetReadOnlyBuffer(data.clusters),
																context.GetReadWriteBuffer(data.light_counter),
																context.GetReadWriteBuffer(data.light_list),
																context.GetReadWriteBuffer(data.light_grid)};
				uint32 src_range_sizes[] = { 1,1,1,1 };
				uint32 i = (uint32)descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(i);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct ClusterCullingConstants
				{
					uint32 clusters_idx;
					uint32 light_index_counter_idx;
					uint32 light_index_list_idx;
					uint32 light_grid_idx;
				} constants =
				{
					.clusters_idx = i, .light_index_counter_idx = i + 1,
					.light_index_list_idx = i + 2, .light_grid_idx = i + 3
				};
				
				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ClusterCulling));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 4, &constants, 0);
				cmd_list->Dispatch(CLUSTER_SIZE_X / 16, CLUSTER_SIZE_Y / 16, CLUSTER_SIZE_Z / 1);

			}, ERGPassType::Compute, ERGPassFlags::None);

		struct ClusteredDeferredLightingPassData
		{
			RGBufferReadOnlyId light_grid;
			RGBufferReadOnlyId light_list;
			RGTextureReadOnlyId gbuffer_normal;
			RGTextureReadOnlyId gbuffer_albedo;
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};
		rendergraph.AddPass<ClusteredDeferredLightingPassData>("Clustered Deferred Lighting Pass",
			[=](ClusteredDeferredLightingPassData& data, RenderGraphBuilder& builder)
			{
				data.gbuffer_normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_PixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_RES_NAME(GBufferAlbedo), ReadAccess_PixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				data.light_grid = builder.ReadBuffer(RG_RES_NAME(LightGrid), ReadAccess_PixelShader);
				data.light_list = builder.ReadBuffer(RG_RES_NAME(LightList), ReadAccess_PixelShader);
				data.output = builder.WriteTexture(RG_RES_NAME(HDR_RenderTarget));
			},
			[=](ClusteredDeferredLightingPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto upload_buffer = gfx->GetDynamicAllocator();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { context.GetReadOnlyBuffer(data.light_list), context.GetReadOnlyBuffer(data.light_grid),
															  context.GetReadOnlyTexture(data.gbuffer_normal), context.GetReadOnlyTexture(data.gbuffer_albedo), 
															  context.GetReadOnlyTexture(data.depth), context.GetReadWriteTexture(data.output) };
				uint32 src_range_sizes[] = { 1,1,1,1,1,1 };
				uint32 i = (uint32)descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
				auto dst_descriptor = descriptor_allocator->GetHandle(i);
				uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
				device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				
				struct ClusteredDeferredLightingConstants
				{
					uint32 light_index_list_idx;
					uint32 light_grid_idx;
					uint32 normal_idx;
					uint32 diffuse_idx;
					uint32 depth_idx;
					uint32 output_idx;
				} constants = 
				{
					.light_index_list_idx = i, .light_grid_idx = i + 1, .normal_idx = i + 2,
					.diffuse_idx = i + 3, .depth_idx = i + 4, .output_idx = i + 5
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::ClusteredDeferredLighting));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 6, &constants, 0);
				cmd_list->Dispatch((UINT)std::ceil(width / 16.0f), (UINT)std::ceil(height / 16.0f), 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

}



