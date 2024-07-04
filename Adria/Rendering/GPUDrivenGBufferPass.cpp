#include "GPUDrivenGBufferPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h"
#include "RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"
#include "Logging/Logger.h"
#include "Editor/GUICommand.h"

#define A_CPU 1
#include "Resources/Shaders/SPD/ffx_a.h"
#include "Resources/Shaders/SPD/ffx_spd.h"

using namespace DirectX;

namespace adria
{
	static constexpr uint32 MAX_NUM_MESHLETS = 1 << 20u;
	static constexpr uint32 MAX_NUM_INSTANCES = 1 << 14u;
	struct MeshletCandidate
	{
		uint32 instance_id;
		uint32 meshlet_index;
	};

	GPUDrivenGBufferPass::GPUDrivenGBufferPass(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height) 
		: reg(reg), gfx(gfx), width(width), height(height)
	{
		CreateDebugBuffer();
		InitializeHZB();
	}

	void GPUDrivenGBufferPass::Render(RenderGraph& rg)
	{
		AddClearCountersPass(rg);
		Add1stPhasePasses(rg);
		Add2ndPhasePasses(rg);
		AddDebugPass(rg);

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNodeEx("GPU Driven Rendering", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Occlusion Cull", &occlusion_culling);
					ImGui::Checkbox("Display Debug Stats", &display_debug_stats);
					if (display_debug_stats)
					{
						ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
						ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

						ImGui::SeparatorText("GPU Driven Debug Stats");
						{
							uint32 backbuffer_index = gfx->GetBackbufferIndex();
							DebugStats current_debug_stats = debug_stats[backbuffer_index];

							ImGui::BeginTable("Profiler", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg);
							ImGui::TableSetupColumn("Description");
							ImGui::TableSetupColumn("Count");
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Total Instances");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.num_instances);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Occluded Instances");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.occluded_instances);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Visible Instances");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.visible_instances);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Phase 1 Candidate Meshlets");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.phase1_candidate_meshlets);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Phase 1 Visible Meshlets");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.phase1_visible_meshlets);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Phase 2 Candidate Meshlets");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.phase2_candidate_meshlets);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Phase 2 Visible Meshlets");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.phase2_visible_meshlets);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Processed");
								ImGui::TableSetColumnIndex(1);
								ImGui::Text("%u", current_debug_stats.processed_meshlets);
							}
							ImGui::EndTable();
						}
					}
					ImGui::TreePop();
					ImGui::Separator();
				}

				
			}, GUICommandGroup_Renderer
		);
	}

	void GPUDrivenGBufferPass::InitializeHZB()
	{
		CalculateHZBParameters();

		GfxTextureDesc hzb_desc{};
		hzb_desc.width = hzb_width;
		hzb_desc.height = hzb_height;
		hzb_desc.mip_levels = hzb_mip_count;
		hzb_desc.format = GfxFormat::R32_FLOAT;
		hzb_desc.initial_state = GfxResourceState::ComputeUAV;
		hzb_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;

		HZB = gfx->CreateTexture(hzb_desc);
	}

	void GPUDrivenGBufferPass::AddClearCountersPass(RenderGraph& rg)
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
				cmd_list->GlobalBarrier(GfxResourceState::ComputeUAV, GfxResourceState::ComputeUAV);
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void GPUDrivenGBufferPass::Add1stPhasePasses(RenderGraph& rg)
	{
		rg.ImportTexture(RG_RES_NAME(HZB), HZB.get());

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
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
				data.occluded_instances = builder.WriteBuffer(RG_RES_NAME(OccludedInstances));
				data.occluded_instances_counter = builder.WriteBuffer(RG_RES_NAME(OccludedInstancesCounter));
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

				uint32 const num_instances = (uint32)reg.view<Batch>().size();
				struct CullInstances1stPhaseConstants
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
				GfxPipelineStateID pso_id = occlusion_culling ? GfxPipelineStateID::CullInstances1stPhase : GfxPipelineStateID::CullInstances1stPhase_NoOcclusionCull;
				cmd_list->SetPipelineState(PSOCache::Get(pso_id));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(num_instances / 64.0f), 1, 1);

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


		struct CullMeshletsPassData
		{
			RGTextureReadOnlyId hzb;
			RGBufferIndirectArgsId indirect_args;
			RGBufferReadWriteId candidate_meshlets;
			RGBufferReadWriteId candidate_meshlets_counter;
			RGBufferReadWriteId visible_meshlets;
			RGBufferReadWriteId visible_meshlets_counter;
		};

		rg.AddPass<CullMeshletsPassData>("1st Phase Cull Meshlets Pass",
			[=](CullMeshletsPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc visible_meshlets_buffer_desc{};
				visible_meshlets_buffer_desc.resource_usage = GfxResourceUsage::Default;
				visible_meshlets_buffer_desc.misc_flags = GfxBufferMiscFlag::BufferStructured;
				visible_meshlets_buffer_desc.stride = sizeof(MeshletCandidate);
				visible_meshlets_buffer_desc.size = sizeof(MeshletCandidate) * MAX_NUM_MESHLETS;
				builder.DeclareBuffer(RG_RES_NAME(VisibleMeshlets), visible_meshlets_buffer_desc);

				data.hzb = builder.ReadTexture(RG_RES_NAME(HZB));
				data.indirect_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(MeshletCullArgs));
				data.candidate_meshlets = builder.WriteBuffer(RG_RES_NAME(CandidateMeshlets));
				data.candidate_meshlets_counter = builder.WriteBuffer(RG_RES_NAME(CandidateMeshletsCounter));
				data.visible_meshlets = builder.WriteBuffer(RG_RES_NAME(VisibleMeshlets));
				data.visible_meshlets_counter = builder.WriteBuffer(RG_RES_NAME(VisibleMeshletsCounter));
			},
			[=](CullMeshletsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { ctx.GetReadOnlyTexture(data.hzb),
												ctx.GetReadWriteBuffer(data.candidate_meshlets),
												ctx.GetReadWriteBuffer(data.candidate_meshlets_counter),
												ctx.GetReadWriteBuffer(data.visible_meshlets),
												ctx.GetReadWriteBuffer(data.visible_meshlets_counter)};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct CullMeshlets1stPhaseConstants
				{
					uint32 hzb_idx;
					uint32 candidate_meshlets_idx;
					uint32 candidate_meshlets_counter_idx;
					uint32 visible_meshlets_idx;
					uint32 visible_meshlets_counter_idx;
				} constants =
				{
					.hzb_idx = i,
					.candidate_meshlets_idx = i + 1,
					.candidate_meshlets_counter_idx = i + 2,
					.visible_meshlets_idx = i + 3,
					.visible_meshlets_counter_idx = i + 4,
				};

				GfxPipelineStateID pso_id = occlusion_culling ? GfxPipelineStateID::CullMeshlets1stPhase : GfxPipelineStateID::CullMeshlets1stPhase_NoOcclusionCull;
				cmd_list->SetPipelineState(PSOCache::Get(pso_id));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);

				GfxBuffer const& indirect_args = ctx.GetIndirectArgsBuffer(data.indirect_args);
				cmd_list->DispatchIndirect(indirect_args, 0);
			}, RGPassType::Compute, RGPassFlags::None);

		struct BuildMeshletDrawArgsPassData
		{
			RGBufferReadOnlyId  visible_meshlets_counter;
			RGBufferReadWriteId meshlet_draw_args;
		};

		rg.AddPass<BuildMeshletDrawArgsPassData>("1st Phase Build Meshlet Draw Args Pass",
			[=](BuildMeshletDrawArgsPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc meshlet_cull_draw_desc{};
				meshlet_cull_draw_desc.resource_usage = GfxResourceUsage::Default;
				meshlet_cull_draw_desc.misc_flags = GfxBufferMiscFlag::IndirectArgs;
				meshlet_cull_draw_desc.stride = sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
				meshlet_cull_draw_desc.size = sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
				builder.DeclareBuffer(RG_RES_NAME(MeshletDrawArgs), meshlet_cull_draw_desc);

				data.meshlet_draw_args = builder.WriteBuffer(RG_RES_NAME(MeshletDrawArgs));
				data.visible_meshlets_counter = builder.ReadBuffer(RG_RES_NAME(VisibleMeshletsCounter));
			},
			[=](BuildMeshletDrawArgsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { ctx.GetReadOnlyBuffer(data.visible_meshlets_counter),
												ctx.GetReadWriteBuffer(data.meshlet_draw_args)
				};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct BuildMeshletDrawArgsConstants
				{
					uint32 visible_meshlets_counter_idx;
					uint32 meshlet_draw_args_idx;
				} constants =
				{
					.visible_meshlets_counter_idx = i + 0,
					.meshlet_draw_args_idx = i + 1
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BuildMeshletDrawArgs1stPhase));
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);

			}, RGPassType::Compute, RGPassFlags::None);

		struct DrawMeshletsPassData
		{
			RGBufferReadOnlyId visible_meshlets;
			RGBufferIndirectArgsId draw_args;
		};
		rg.AddPass<DrawMeshletsPassData>("Draw Meshlets",
			[=](DrawMeshletsPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc gbuffer_desc{};
				gbuffer_desc.width = width;
				gbuffer_desc.height = height;
				gbuffer_desc.format = GfxFormat::R8G8B8A8_UNORM;
				gbuffer_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);

				builder.DeclareTexture(RG_RES_NAME(GBufferNormal), gbuffer_desc);
				builder.DeclareTexture(RG_RES_NAME(GBufferAlbedo), gbuffer_desc);
				builder.DeclareTexture(RG_RES_NAME(GBufferEmissive), gbuffer_desc);

				builder.WriteRenderTarget(RG_RES_NAME(GBufferNormal), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), RGLoadStoreAccessOp::Clear_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferEmissive), RGLoadStoreAccessOp::Clear_Preserve);

				RGTextureDesc depth_desc{};
				depth_desc.width = width;
				depth_desc.height = height;
				depth_desc.format = GfxFormat::D32_FLOAT;
				depth_desc.clear_value = GfxClearValue(0.0f, 0);
				builder.DeclareTexture(RG_RES_NAME(DepthStencil), depth_desc);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Clear_Preserve);
				builder.SetViewport(width, height);

				data.visible_meshlets = builder.ReadBuffer(RG_RES_NAME(VisibleMeshlets));
				data.draw_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(MeshletDrawArgs));
			},
			[=](DrawMeshletsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] =
				{
					ctx.GetReadOnlyBuffer(data.visible_meshlets)
				};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct DrawMeshlets1stPhaseConstants
				{
					uint32 visible_meshlets_idx;
				} constants =
				{
					.visible_meshlets_idx = i,
				};

				GfxPipelineStateID pso_id = rain_active ? GfxPipelineStateID::DrawMeshlets_Rain : GfxPipelineStateID::DrawMeshlets;
				cmd_list->SetPipelineState(PSOCache::Get(pso_id));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				GfxBuffer const& draw_args = ctx.GetIndirectArgsBuffer(data.draw_args);
				cmd_list->DispatchMeshIndirect(draw_args, 0);
			}, RGPassType::Graphics, RGPassFlags::None);

		AddHZBPasses(rg);
	}

	void GPUDrivenGBufferPass::Add2ndPhasePasses(RenderGraph& rg)
	{
		if (!occlusion_culling) return;

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct BuildInstanceCullArgsPassData
		{
			RGBufferReadOnlyId  occluded_instances_counter;
			RGBufferReadWriteId instance_cull_args;
		};

		rg.AddPass<BuildInstanceCullArgsPassData>("2nd Phase Build Instance Cull Args Pass",
			[=](BuildInstanceCullArgsPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc instance_cull_args_desc{};
				instance_cull_args_desc.resource_usage = GfxResourceUsage::Default;
				instance_cull_args_desc.misc_flags = GfxBufferMiscFlag::IndirectArgs;
				instance_cull_args_desc.stride = sizeof(D3D12_DISPATCH_ARGUMENTS);
				instance_cull_args_desc.size = sizeof(D3D12_DISPATCH_ARGUMENTS);
				builder.DeclareBuffer(RG_RES_NAME(InstanceCullArgs), instance_cull_args_desc);

				data.instance_cull_args = builder.WriteBuffer(RG_RES_NAME(InstanceCullArgs));
				data.occluded_instances_counter = builder.ReadBuffer(RG_RES_NAME(OccludedInstancesCounter));
			},
			[=](BuildInstanceCullArgsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { ctx.GetReadOnlyBuffer(data.occluded_instances_counter),
												ctx.GetReadWriteBuffer(data.instance_cull_args)
				};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct BuildInstanceCullArgsConstants
				{
					uint32  occluded_instances_counter_idx;
					uint32  instance_cull_args_idx;
				} constants =
				{
					.occluded_instances_counter_idx = i + 0,
					.instance_cull_args_idx = i + 1
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BuildInstanceCullArgs));
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);

			}, RGPassType::Compute, RGPassFlags::None);

		struct CullInstancesPassData
		{
			RGTextureReadOnlyId hzb;
			RGBufferIndirectArgsId cull_args;
			RGBufferReadWriteId candidate_meshlets;
			RGBufferReadWriteId candidate_meshlets_counter;
			RGBufferReadWriteId occluded_instances;
			RGBufferReadWriteId occluded_instances_counter;
		};

		rg.AddPass<CullInstancesPassData>("2nd Phase Cull Instances Pass",
			[=](CullInstancesPassData& data, RenderGraphBuilder& builder)
			{
				data.hzb = builder.ReadTexture(RG_RES_NAME(HZB));
				data.cull_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(InstanceCullArgs));
				data.occluded_instances = builder.WriteBuffer(RG_RES_NAME(OccludedInstances));
				data.occluded_instances_counter = builder.WriteBuffer(RG_RES_NAME(OccludedInstancesCounter));
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

				struct CullInstances2ndPhaseConstants
				{
					uint32 num_instances;
					uint32 hzb_idx;
					uint32 occluded_instances_idx;
					uint32 occluded_instances_counter_idx;
					uint32 candidate_meshlets_idx;
					uint32 candidate_meshlets_counter_idx;
				} constants =
				{
					.num_instances = 0,
					.hzb_idx = i,
					.occluded_instances_idx = i + 1,
					.occluded_instances_counter_idx = i + 2,
					.candidate_meshlets_idx = i + 3,
					.candidate_meshlets_counter_idx = i + 4,
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::CullInstances2ndPhase));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				GfxBuffer const& dispatch_args = ctx.GetIndirectArgsBuffer(data.cull_args);
				cmd_list->DispatchIndirect(dispatch_args, 0);

			}, RGPassType::Compute, RGPassFlags::None);

		struct BuildMeshletCullArgsPassData
		{
			RGBufferReadOnlyId  candidate_meshlets_counter;
			RGBufferReadWriteId meshlet_cull_args;
		};

		rg.AddPass<BuildMeshletCullArgsPassData>("2nd Phase Build Meshlet Cull Args Pass",
			[=](BuildMeshletCullArgsPassData& data, RenderGraphBuilder& builder)
			{
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
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BuildMeshletCullArgs2ndPhase));
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);

			}, RGPassType::Compute, RGPassFlags::None);


		struct CullMeshletsPassData
		{
			RGTextureReadOnlyId hzb;
			RGBufferIndirectArgsId indirect_args;
			RGBufferReadWriteId candidate_meshlets;
			RGBufferReadWriteId candidate_meshlets_counter;
			RGBufferReadWriteId visible_meshlets;
			RGBufferReadWriteId visible_meshlets_counter;
		};

		rg.AddPass<CullMeshletsPassData>("2nd Phase Cull Meshlets Pass",
			[=](CullMeshletsPassData& data, RenderGraphBuilder& builder)
			{
				data.hzb = builder.ReadTexture(RG_RES_NAME(HZB));
				data.indirect_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(MeshletCullArgs));
				data.candidate_meshlets = builder.WriteBuffer(RG_RES_NAME(CandidateMeshlets));
				data.candidate_meshlets_counter = builder.WriteBuffer(RG_RES_NAME(CandidateMeshletsCounter));
				data.visible_meshlets = builder.WriteBuffer(RG_RES_NAME(VisibleMeshlets));
				data.visible_meshlets_counter = builder.WriteBuffer(RG_RES_NAME(VisibleMeshletsCounter));
			},
			[=](CullMeshletsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { ctx.GetReadOnlyTexture(data.hzb),
												ctx.GetReadWriteBuffer(data.candidate_meshlets),
												ctx.GetReadWriteBuffer(data.candidate_meshlets_counter),
												ctx.GetReadWriteBuffer(data.visible_meshlets),
												ctx.GetReadWriteBuffer(data.visible_meshlets_counter) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct CullMeshlets2ndPhaseConstants
				{
					uint32 hzb_idx;
					uint32 candidate_meshlets_idx;
					uint32 candidate_meshlets_counter_idx;
					uint32 visible_meshlets_idx;
					uint32 visible_meshlets_counter_idx;
				} constants =
				{
					.hzb_idx = i,
					.candidate_meshlets_idx = i + 1,
					.candidate_meshlets_counter_idx = i + 2,
					.visible_meshlets_idx = i + 3,
					.visible_meshlets_counter_idx = i + 4,
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::CullMeshlets2ndPhase));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);

				GfxBuffer const& indirect_args = ctx.GetIndirectArgsBuffer(data.indirect_args);
				cmd_list->DispatchIndirect(indirect_args, 0);
			}, RGPassType::Compute, RGPassFlags::None);

		struct BuildMeshletDrawArgsPassData
		{
			RGBufferReadOnlyId  visible_meshlets_counter;
			RGBufferReadWriteId meshlet_draw_args;
		};

		rg.AddPass<BuildMeshletDrawArgsPassData>("2nd Phase Build Meshlet Draw Args Pass",
			[=](BuildMeshletDrawArgsPassData& data, RenderGraphBuilder& builder)
			{
				data.meshlet_draw_args = builder.WriteBuffer(RG_RES_NAME(MeshletDrawArgs));
				data.visible_meshlets_counter = builder.ReadBuffer(RG_RES_NAME(VisibleMeshletsCounter));
			},
			[=](BuildMeshletDrawArgsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { ctx.GetReadOnlyBuffer(data.visible_meshlets_counter),
												ctx.GetReadWriteBuffer(data.meshlet_draw_args)
				};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct BuildMeshletDrawArgsConstants
				{
					uint32 visible_meshlets_counter_idx;
					uint32 meshlet_draw_args_idx;
				} constants =
				{
					.visible_meshlets_counter_idx = i + 0,
					.meshlet_draw_args_idx = i + 1
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::BuildMeshletDrawArgs2ndPhase));
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(1, 1, 1);

			}, RGPassType::Compute, RGPassFlags::None);

		struct DrawMeshletsPassData
		{
			RGBufferReadOnlyId visible_meshlets;
			RGBufferIndirectArgsId draw_args;
		};
		rg.AddPass<DrawMeshletsPassData>("Draw Meshlets",
			[=](DrawMeshletsPassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(GBufferNormal), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferAlbedo), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteRenderTarget(RG_RES_NAME(GBufferEmissive), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);

				data.visible_meshlets = builder.ReadBuffer(RG_RES_NAME(VisibleMeshlets));
				data.draw_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME(MeshletDrawArgs));
			},
			[=](DrawMeshletsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] =
				{
					ctx.GetReadOnlyBuffer(data.visible_meshlets)
				};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct DrawMeshlets1stPhaseConstants
				{
					uint32 visible_meshlets_idx;
				} constants =
				{
					.visible_meshlets_idx = i,
				};

				GfxPipelineStateID pso_id = rain_active ? GfxPipelineStateID::DrawMeshlets_Rain : GfxPipelineStateID::DrawMeshlets;
				cmd_list->SetPipelineState(PSOCache::Get(pso_id));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				GfxBuffer const& draw_args = ctx.GetIndirectArgsBuffer(data.draw_args);
				cmd_list->DispatchMeshIndirect(draw_args, 0);
			}, RGPassType::Graphics, RGPassFlags::None);

		AddHZBPasses(rg, true);
	}

	void GPUDrivenGBufferPass::AddHZBPasses(RenderGraph& rg, bool second_phase)
	{
		if (!occlusion_culling) return;

		struct InitializeHZBPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId hzb;
		};
		std::string hzb_init_name = "HZB Init ";
		hzb_init_name += (second_phase ? "2nd phase" : "1st phase");
		rg.AddPass<InitializeHZBPassData>(hzb_init_name.c_str(),
			[=](InitializeHZBPassData& data, RenderGraphBuilder& builder)
			{
				data.hzb = builder.WriteTexture(RG_RES_NAME(HZB));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil));
			},
			[=](InitializeHZBPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = {
					ctx.GetReadOnlyTexture(data.depth),
					ctx.GetReadWriteTexture(data.hzb)
				};
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				struct InitializeHZBConstants
				{
					uint32 depth_idx;
					uint32 hzb_idx;
					float inv_hzb_width;
					float inv_hzb_height;
				} constants =
				{
					.depth_idx = i,
					.hzb_idx = i + 1,
					.inv_hzb_width = 1.0f / hzb_width,
					.inv_hzb_height = 1.0f / hzb_height
				};
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::InitializeHZB));
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch((uint32)std::ceil(hzb_width / 16.0f), (uint32)std::ceil(hzb_width / 16.0f), 1);
			}, RGPassType::Compute, RGPassFlags::ForceNoCull);

		struct HZBMipsPassData
		{
			RGBufferReadWriteId  spd_counter;
			RGTextureReadWriteId hzb_mips[12];
		};

		std::string hzb_mips_name = "HZB Mips ";
		hzb_mips_name += (second_phase ? "2nd phase" : "1st phase");
		rg.AddPass<HZBMipsPassData>(hzb_mips_name.c_str(),
			[=](HZBMipsPassData& data, RenderGraphBuilder& builder)
			{
				if (!second_phase)
				{
					RGBufferDesc counter_desc{};
					counter_desc.size = sizeof(uint32);
					counter_desc.format = GfxFormat::R32_UINT;
					counter_desc.stride = sizeof(uint32);
					builder.DeclareBuffer(RG_RES_NAME(SPDCounter), counter_desc);
				}

				ADRIA_ASSERT(hzb_mip_count <= 12);
				for (uint32 i = 0; i < hzb_mip_count; ++i)
				{
					data.hzb_mips[i] = builder.WriteTexture(RG_RES_NAME(HZB), i, 1);
				}
				data.spd_counter = builder.WriteBuffer(RG_RES_NAME(SPDCounter));
			},
			[=](HZBMipsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				varAU2(dispatchThreadGroupCountXY);
				varAU2(workGroupOffset);
				varAU2(numWorkGroupsAndMips);
				varAU4(rectInfo) = initAU4(0, 0, hzb_width, hzb_height);
				uint32 mips = hzb_mip_count;

				SpdSetup(
					dispatchThreadGroupCountXY,
					workGroupOffset,
					numWorkGroupsAndMips,
					rectInfo,
					mips - 1);

				std::vector<GfxDescriptor> src_handles(hzb_mip_count + 1);
				src_handles[0] = ctx.GetReadWriteBuffer(data.spd_counter);
				for (uint32 i = 0; i < hzb_mip_count; ++i) src_handles[i + 1] = ctx.GetReadWriteTexture(data.hzb_mips[i]);

				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU((uint32)src_handles.size());
				gfx->CopyDescriptors(dst_handle, src_handles);
				uint32 i = dst_handle.GetIndex();

				GfxDescriptor counter_uav_cpu = src_handles[0];
				GfxDescriptor counter_uav_gpu = dst_handle;
				GfxBuffer& spd_counter = ctx.GetBuffer(*data.spd_counter);
				uint32 clear[] = { 0u };
				cmd_list->ClearUAV(spd_counter, counter_uav_gpu, counter_uav_cpu, clear);
				cmd_list->GlobalBarrier(GfxResourceState::ComputeUAV, GfxResourceState::ComputeUAV);

				struct HZBMipsConstants
				{
					uint32 num_mips;
					uint32 num_work_groups;
					uint32 work_group_offset_x;
					uint32 work_group_offset_y;
				} constants
				{
					.num_mips = numWorkGroupsAndMips[1],
					.num_work_groups = numWorkGroupsAndMips[0],
					.work_group_offset_x = workGroupOffset[0],
					.work_group_offset_y = workGroupOffset[1]
				};

				DECLSPEC_ALIGN(16)
				struct SPDIndices
				{
					XMUINT4	dstIdx[12];
					uint32	spdGlobalAtomicIdx;
				} indices{ .spdGlobalAtomicIdx = i };
				for (uint32 j = 0; j < hzb_mip_count; ++j) indices.dstIdx[j].x = i + 1 + j;

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::HZBMips));
				cmd_list->SetRootConstants(1, constants);
				cmd_list->SetRootCBV(2, indices);
				cmd_list->Dispatch(dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], 1);
			}, RGPassType::Compute, RGPassFlags::ForceNoCull);
	}

	void GPUDrivenGBufferPass::AddDebugPass(RenderGraph& rg)
	{
		if (!display_debug_stats) return;

		struct GPUDrivenDebugPassData
		{
			RGBufferCopySrcId  candidate_meshlets_counter;
			RGBufferCopySrcId  visible_meshlets_counter;
			RGBufferCopySrcId  occluded_instances_counter;
			RGBufferCopyDstId  debug_buffer;
		};

		rg.ImportBuffer(RG_RES_NAME(GPUDrivenDebugBuffer), debug_buffer.get());

		rg.AddPass<GPUDrivenDebugPassData>("GPU Driven Debug Pass",
			[=](GPUDrivenDebugPassData& data, RenderGraphBuilder& builder)
			{
				data.debug_buffer = builder.WriteCopyDstBuffer(RG_RES_NAME(GPUDrivenDebugBuffer));
				data.candidate_meshlets_counter = builder.ReadCopySrcBuffer(RG_RES_NAME(CandidateMeshletsCounter));
				data.visible_meshlets_counter = builder.ReadCopySrcBuffer(RG_RES_NAME(VisibleMeshletsCounter));
				data.occluded_instances_counter = builder.ReadCopySrcBuffer(RG_RES_NAME(OccludedInstancesCounter));
			},
			[&](GPUDrivenDebugPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxBuffer const& src_buffer1 = context.GetCopySrcBuffer(data.occluded_instances_counter);
				GfxBuffer const& src_buffer2 = context.GetCopySrcBuffer(data.visible_meshlets_counter);
				GfxBuffer const& src_buffer3 = context.GetCopySrcBuffer(data.candidate_meshlets_counter);
				GfxBuffer& debug_buffer = context.GetCopyDstBuffer(data.debug_buffer);

				uint32 backbuffer_index = gfx->GetBackbufferIndex();
				uint32 buffer_offset = 6 * sizeof(uint32) * backbuffer_index;
				cmd_list->CopyBuffer(debug_buffer, buffer_offset, src_buffer1, 0, sizeof(uint32));
				cmd_list->CopyBuffer(debug_buffer, buffer_offset + sizeof(uint32), src_buffer2, 0, 2 * sizeof(uint32));
				cmd_list->CopyBuffer(debug_buffer, buffer_offset + 3 * sizeof(uint32), src_buffer3, 0, 3 * sizeof(uint32));

				ADRIA_ASSERT(debug_buffer.IsMapped());
				uint32* buffer_data = debug_buffer.GetMappedData<uint32>();
				buffer_data += 6 * backbuffer_index;
				uint32 num_instances = (uint32)reg.view<Batch>().size();
				debug_stats[backbuffer_index].occluded_instances = buffer_data[0];
				debug_stats[backbuffer_index].num_instances = num_instances;
				debug_stats[backbuffer_index].visible_instances = num_instances - buffer_data[0];
				debug_stats[backbuffer_index].processed_meshlets = buffer_data[3];
				debug_stats[backbuffer_index].phase1_candidate_meshlets = buffer_data[4];
				debug_stats[backbuffer_index].phase2_candidate_meshlets = buffer_data[5];
				debug_stats[backbuffer_index].phase1_visible_meshlets = buffer_data[1];
				debug_stats[backbuffer_index].phase2_visible_meshlets = buffer_data[2];

			}, RGPassType::Copy, RGPassFlags::ForceNoCull);

	}

	void GPUDrivenGBufferPass::CalculateHZBParameters()
	{
		uint32 mips_x = (uint32)std::max(ceilf(log2f((float)width)), 1.0f);
		uint32 mips_y = (uint32)std::max(ceilf(log2f((float)height)), 1.0f);

		hzb_mip_count = std::max(mips_x, mips_y);
		ADRIA_ASSERT(hzb_mip_count <= MAX_HZB_MIP_COUNT);
		hzb_width = 1 << (mips_x - 1);
		hzb_height = 1 << (mips_y - 1);
	}

	void GPUDrivenGBufferPass::CreateDebugBuffer()
	{
		GfxBufferDesc debug_buffer_desc{};
		debug_buffer_desc.size = 6 * sizeof(uint32) * GFX_BACKBUFFER_COUNT;
		debug_buffer_desc.resource_usage = GfxResourceUsage::Readback;
		debug_buffer = gfx->CreateBuffer(debug_buffer_desc);
	}

}
