#include "GPUDrivenRenderer.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"
#include "RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "Logging/Logger.h"
#include "Core/Defines.h"



namespace adria
{
	static constexpr uint32 MAX_NUM_MESHLETS = 1 << 20u;
	static constexpr uint32 MAX_NUM_INSTANCES = 1 << 14u;

	void GPUDrivenRenderer::Render(RenderGraph& rg)
	{
		rg.ImportTexture(RG_RES_NAME(HZB), HZB.get());
		AddClearCountersPass(rg);
		Add1stPhasePasses(rg);
		Add2ndPhasePasses(rg);
	}

	void GPUDrivenRenderer::InitializeHZB()
	{
		CalculateHZBParameters();

		GfxTextureDesc hzb_desc{};
		hzb_desc.width = hzb_width;
		hzb_desc.height = hzb_height;
		hzb_desc.mip_levels = hzb_mip_count;
		hzb_desc.format = GfxFormat::R16_FLOAT;
		hzb_desc.initial_state = GfxResourceState::NonPixelShaderResource;
		hzb_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;

		HZB = std::make_unique<GfxTexture>(gfx, hzb_desc);
	}

	void GPUDrivenRenderer::AddClearCountersPass(RenderGraph& rg)
	{
		struct ClearCountersPassData
		{
			RGBufferReadWriteId candidate_meshlets_counter;
			RGBufferReadWriteId visible_meshlets_counter;
			RGBufferReadWriteId occluded_instances_counter;
		};

		rg.AddPass<ClearCountersPassData>("Clear Counters Pass",
			[=](ClearCountersPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc counter_desc{};
				counter_desc.size = 3 * sizeof(uint32);
				counter_desc.format = GfxFormat::R32_UINT;
				counter_desc.stride = sizeof(uint32);
				builder.DeclareBuffer(RG_RES_NAME(CandidateMeshletsCounter), counter_desc);
				data.candidate_meshlets_counter = builder.WriteBuffer(RG_RES_NAME(CandidateMeshletsCounter));

				counter_desc.size = 2 * sizeof(uint32);
				builder.DeclareBuffer(RG_RES_NAME(VisibleMeshletsCounter), counter_desc);
				data.visible_meshlets_counter = builder.WriteBuffer(RG_RES_NAME(VisibleMeshletsCounter));

				counter_desc.size = sizeof(uint32);
				builder.DeclareBuffer(RG_RES_NAME(OccludedInstancesCounter), counter_desc);
				data.occluded_instances_counter = builder.WriteBuffer(RG_RES_NAME(OccludedInstancesCounter));
			},
			[=](ClearCountersPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(3);
				GfxDescriptor src_handles[] = { ctx.GetReadWriteBuffer(data.candidate_meshlets_counter),
												ctx.GetReadWriteBuffer(data.visible_meshlets_counter),
												ctx.GetReadWriteBuffer(data.occluded_instances_counter) };
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct ClearCountersConstants
				{
					uint32 candidate_meshlets_counter_idx;
					uint32 visible_meshlets_counter_idx;
					uint32 occluded_instances_counter_idx;
				} constants =
				{
					.candidate_meshlets_counter_idx = i,
					.visible_meshlets_counter_idx = i + 1,
					.occluded_instances_counter_idx = i + 2
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::ClearCounters)); 
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);
				cmd_list->UavBarrier();
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void GPUDrivenRenderer::Add1stPhasePasses(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();

		struct CullInstancesPassData
		{
			RGTextureReadOnlyId hzb;
			RGBufferReadWriteId candidate_meshlets;
			RGBufferReadWriteId candidate_meshlets_counter;
			RGBufferReadWriteId occluded_instances;
			RGBufferReadWriteId occluded_instances_counter;
		};

		rg.AddPass<CullInstancesPassData>("1st Phase Cull Instances Pass",
			[=](CullInstancesPassData& data, RenderGraphBuilder& builder)
			{
				struct MeshletCandidate
				{
					uint32 instance_id;
					uint32 meshlet_index;
				};

				RGBufferDesc candidate_meshlets_buffer_desc{};
				candidate_meshlets_buffer_desc.resource_usage = GfxResourceUsage::Default;
				candidate_meshlets_buffer_desc.misc_flags = GfxBufferMiscFlag::BufferStructured;
				candidate_meshlets_buffer_desc.stride = sizeof(MeshletCandidate);
				candidate_meshlets_buffer_desc.size = sizeof(MeshletCandidate) * MAX_NUM_MESHLETS;
				builder.DeclareBuffer(RG_RES_NAME(CandidateMeshlets), candidate_meshlets_buffer_desc);

				RGBufferDesc occluded_instances_buffer_desc{};
				occluded_instances_buffer_desc.resource_usage = GfxResourceUsage::Default;
				occluded_instances_buffer_desc.misc_flags = GfxBufferMiscFlag::BufferStructured;
				occluded_instances_buffer_desc.stride = sizeof(uint32);
				occluded_instances_buffer_desc.size = sizeof(uint32) * MAX_NUM_INSTANCES;
				builder.DeclareBuffer(RG_RES_NAME(OccludedInstances), occluded_instances_buffer_desc);

				data.hzb = builder.ReadTexture(RG_RES_NAME(HZB));
				data.occluded_instances_counter = builder.WriteBuffer(RG_RES_NAME(OccludedInstancesCounter));
				data.occluded_instances = builder.WriteBuffer(RG_RES_NAME(CandidateMeshlets));
				data.candidate_meshlets = builder.WriteBuffer(RG_RES_NAME(CandidateMeshlets));
				data.candidate_meshlets_counter = builder.WriteBuffer(RG_RES_NAME(CandidateMeshletsCounter));
			},
			[=](CullInstancesPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { ctx.GetReadOnlyTexture(data.hzb),
												ctx.GetReadWriteBuffer(data.occluded_instances),
												ctx.GetReadWriteBuffer(data.occluded_instances_counter),
												ctx.GetReadWriteBuffer(data.candidate_meshlets),
												ctx.GetReadWriteBuffer(data.candidate_meshlets_counter) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				uint32 const num_instances = reg.view<Batch>().size();
				struct CullInstancesConstants
				{
					uint32 num_instances;
					uint32 hzb_idx;
					uint32 occluded_instances_idx;
					uint32 occluded_instances_counter_idx;
					uint32 candidate_meshlets_idx;
					uint32 candidate_meshlets_counter_idx;
				} constants =
				{
					.num_instances = num_instances,
					.hzb_idx = i,
					.occluded_instances_idx = i + 1,
					.occluded_instances_counter_idx = i + 2,
					.candidate_meshlets_idx = i + 3,
					.candidate_meshlets_counter_idx = i + 4,
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::CullInstances1stPhase));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(num_instances / 64), 1, 1);

			}, RGPassType::Compute, RGPassFlags::None);

		struct BuildMeshletCullArgsPassData
		{
			RGBufferReadOnlyId  candidate_meshlets_counter;
			RGBufferReadWriteId meshlet_cull_args;
		};

		rg.AddPass<BuildMeshletCullArgsPassData>("1st Phase Build Meshlet Cull Args Pass",
			[=](BuildMeshletCullArgsPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc meshlet_cull_args_desc{};
				meshlet_cull_args_desc.resource_usage = GfxResourceUsage::Default;
				meshlet_cull_args_desc.misc_flags = GfxBufferMiscFlag::IndirectArgs;
				meshlet_cull_args_desc.stride = sizeof(D3D12_DISPATCH_ARGUMENTS);
				meshlet_cull_args_desc.size = sizeof(D3D12_DISPATCH_ARGUMENTS);
				builder.DeclareBuffer(RG_RES_NAME(MeshletCullArgs), meshlet_cull_args_desc);

				data.meshlet_cull_args = builder.WriteBuffer(RG_RES_NAME(MeshletCullArgs));
				data.candidate_meshlets_counter = builder.ReadBuffer(RG_RES_NAME(CandidateMeshletsCounter));
			},
			[=](BuildMeshletCullArgsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { ctx.GetReadOnlyBuffer(data.candidate_meshlets_counter),
												ctx.GetReadWriteBuffer(data.meshlet_cull_args)
				};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct BuildMeshletCullArgsConstants
				{
					uint32 candidate_meshlets_counter_idx;
					uint32 meshlet_cull_args_idx;
				} constants =
				{
					.candidate_meshlets_counter_idx = i + 0,
					.meshlet_cull_args_idx = i + 1
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BuildMeshletCullArgs1stPhase));
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);

			}, RGPassType::Compute, RGPassFlags::None);


		//meshlet cull

		//draw

		//build hzb
	}

	void GPUDrivenRenderer::Add2ndPhasePasses(RenderGraph& rg)
	{

	}

	void GPUDrivenRenderer::CalculateHZBParameters()
	{
		uint32 mips_x = (uint32)std::max(ceilf(log2f((float)width)), 1.0f);
		uint32 mips_y = (uint32)std::max(ceilf(log2f((float)height)), 1.0f);

		hzb_mip_count = std::max(mips_x, mips_y);
		ADRIA_ASSERT(hzb_mip_count <= MAX_HZB_MIP_COUNT);
		hzb_width = 1 << (mips_x - 1);
		hzb_height = 1 << (mips_y - 1);
	}
}