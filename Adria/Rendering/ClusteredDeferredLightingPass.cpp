#include "ClusteredDeferredLightingPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Logging/Logger.h"
#include "entt/entity/registry.hpp"

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

	ClusteredDeferredLightingPass::ClusteredDeferredLightingPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h) : reg(reg), width(w), height(h),
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
				[=](ClusterBuildingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();
					

					GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, context.GetReadWriteBuffer(data.clusters));

					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::ClusterBuilding));
					cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
					cmd_list->SetRootConstant(1, dst_descriptor.GetIndex(), 0);
					cmd_list->Dispatch(CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
				}, RGPassType::Compute, RGPassFlags::None);
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
			[=](ClusterCullingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { context.GetReadOnlyBuffer(data.clusters),
												context.GetReadWriteBuffer(data.light_counter),
												context.GetReadWriteBuffer(data.light_list),
												context.GetReadWriteBuffer(data.light_grid) };
	
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);

				uint32 i = dst_handle.GetIndex();
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

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::ClusterCulling));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(CLUSTER_SIZE_X / 16, CLUSTER_SIZE_Y / 16, CLUSTER_SIZE_Z / 1);

			}, RGPassType::Compute, RGPassFlags::None);

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
			[=](ClusteredDeferredLightingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { context.GetReadOnlyBuffer(data.light_list), context.GetReadOnlyBuffer(data.light_grid),
															  context.GetReadOnlyTexture(data.gbuffer_normal), context.GetReadOnlyTexture(data.gbuffer_albedo), 
															  context.GetReadOnlyTexture(data.depth), context.GetReadWriteTexture(data.output) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				uint32 i = dst_handle.GetIndex();
				gfx->CopyDescriptors(dst_handle, src_handles);
				
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

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::ClusteredDeferredLighting));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(width / 16.0f), (uint32)std::ceil(height / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::None);
	}

}



