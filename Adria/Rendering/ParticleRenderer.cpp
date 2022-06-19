#include "ParticleRenderer.h"
#include "Enums.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../tecs/registry.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Utilities/Random.h"
#include "../Logging/Logger.h"

namespace adria
{

	ParticleRenderer::ParticleRenderer(tecs::registry& reg, GraphicsDevice* gfx, TextureManager& texture_manager, uint32 w, uint32 h)
		: reg(reg), gfx(gfx), texture_manager(texture_manager), width(w), height(h)
	{
	}

	void ParticleRenderer::Update(float32 dt)
	{
		auto emitters = reg.view<Emitter>();
		for (auto emitter : emitters)
		{
			Emitter& emitter_params = emitters.get(emitter);
			emitter_params.elapsed_time += dt;
			if (emitter_params.particles_per_second > 0.0f)
			{
				emitter_params.accumulation += emitter_params.particles_per_second * dt;

				if (emitter_params.accumulation > 1.0f)
				{
					float64 integer_part = 0.0;
					float32 fraction = (float32)modf(emitter_params.accumulation, &integer_part);

					emitter_params.number_to_emit = (int32)integer_part;
					emitter_params.accumulation = fraction;
				}
			}
		}
	}

	void ParticleRenderer::AddPasses(RenderGraph& rg)
	{

		auto emitters = reg.view<Emitter>();
		for (auto emitter : emitters)
		{
			Emitter const& emitter_params = emitters.get(emitter);
			if (emitter_params.pause) continue;

			size_t id = tecs::as_integer(emitter);
			rg.ImportBuffer(RG_RES_NAME_IDX(DeadListBuffer, id), dead_list_buffer_map[id].get());
			rg.ImportBuffer(RG_RES_NAME_IDX(AliveIndexBuffer, id), alive_index_buffer_map[id].get());
			rg.ImportBuffer(RG_RES_NAME_IDX(DeadListBufferCounter, id), dead_list_buffer_counter_map[id].get());
			rg.ImportBuffer(RG_RES_NAME_IDX(AliveIndexBufferCounter, id), alive_index_buffer_counter_map[id].get());
			rg.ImportBuffer(RG_RES_NAME_IDX(ParticleBufferA, id), particle_bufferA_map[id].get());
			rg.ImportBuffer(RG_RES_NAME_IDX(ParticleBufferB, id), particle_bufferB_map[id].get());
			rg.ImportBuffer(RG_RES_NAME_IDX(VSPositions, id), view_space_positions_buffer_map[id].get());

			if (emitter_params.reset_emitter)
			{
				AddInitializeDeadListPass(rg, emitter_params, id);
				AddResetParticlesPass(rg, emitter_params, id);
				emitter_params.reset_emitter = false;
			}
			AddEmitPass(rg, emitter_params, id);
			AddSimulatePass(rg, emitter_params, id);
			if (emitter_params.sort)
			{
				AddSortPasses(rg, emitter_params, id);
			}
			AddRasterizePass(rg, emitter_params, id);
		}
	}

	void ParticleRenderer::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void ParticleRenderer::OnSceneInitialized()
	{
		ID3D12Device* device = gfx->GetDevice();

		D3D12_INDIRECT_ARGUMENT_DESC args[1] = {};
		args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC command_signature_desc{};
		command_signature_desc.NumArgumentDescs = 1;
		command_signature_desc.pArgumentDescs = args;
		command_signature_desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
		BREAK_IF_FAILED(device->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&indirect_render_args_signature)));

		args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
		command_signature_desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
		BREAK_IF_FAILED(device->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&indirect_sort_args_signature)));

		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		std::vector<float32> random_texture_data(1024 * 1024 * 4);
		for (uint32 i = 0; i < random_texture_data.size(); i++)
		{
			random_texture_data[i] = 2.0f * rand_float() - 1.0f;
		}

		D3D12_SUBRESOURCE_DATA initial_data{};
		initial_data.pData = random_texture_data.data();
		initial_data.RowPitch = 1024 * 4 * sizeof(float32);
		initial_data.SlicePitch = 0;

		TextureDesc noise_desc{};
		noise_desc.width = 1024;
		noise_desc.height = 1024;
		noise_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		noise_desc.bind_flags = EBindFlag::ShaderResource;
		noise_desc.initial_state = EResourceState::CopyDest;
		random_texture = std::make_unique<Texture>(gfx, noise_desc, &initial_data);
		random_texture->CreateSubresource_SRV();

		std::vector<uint32> indices(MAX_PARTICLES * 6);
		uint32 base = 0;
		size_t offset = 0;
		for (size_t i = 0; i < MAX_PARTICLES; i++)
		{
			indices[offset + 0] = base + 0;
			indices[offset + 1] = base + 1;
			indices[offset + 2] = base + 2;

			indices[offset + 3] = base + 2;
			indices[offset + 4] = base + 1;
			indices[offset + 5] = base + 3;

			base += 4;
			offset += 6;
		}
		BufferInitialData ib_initial_data = indices.data();

		BufferDesc ib_desc{};
		ib_desc.bind_flags = EBindFlag::None;
		ib_desc.misc_flags = EBufferMiscFlag::IndexBuffer;
		ib_desc.size = indices.size() * sizeof(uint32);
		ib_desc.stride = sizeof(uint32);
		ib_desc.format = DXGI_FORMAT_R32_UINT;
		index_buffer = std::make_unique<Buffer>(gfx, ib_desc, ib_initial_data);

		BufferDesc reset_buffer_desc{};
		reset_buffer_desc.size = sizeof(uint32);
		reset_buffer_desc.resource_usage = EResourceUsage::Upload;
		uint32 initial_reset_data[] = { 0 };
		counter_reset_buffer = std::make_unique<Buffer>(gfx, reset_buffer_desc, initial_reset_data);
	}

	void ParticleRenderer::OnEmitterAdded(size_t id)
	{
		dead_list_buffer_map.emplace(std::make_pair(id, std::make_unique<Buffer>(gfx, StructuredBufferDesc<uint32>(MAX_PARTICLES))));
		alive_index_buffer_map.emplace(std::make_pair(id, std::make_unique<Buffer>(gfx, StructuredBufferDesc<IndexBufferElement>(MAX_PARTICLES))));
		dead_list_buffer_counter_map.emplace(std::make_pair(id, std::make_unique<Buffer>(gfx, CounterBufferDesc())));
		alive_index_buffer_counter_map.emplace(std::make_pair(id, std::make_unique<Buffer>(gfx, CounterBufferDesc())));
		particle_bufferA_map.emplace(std::make_pair(id, std::make_unique<Buffer>(gfx, StructuredBufferDesc<GPUParticleA>(MAX_PARTICLES))));
		particle_bufferB_map.emplace(std::make_pair(id, std::make_unique<Buffer>(gfx, StructuredBufferDesc<GPUParticleB>(MAX_PARTICLES))));
		view_space_positions_buffer_map.emplace(std::make_pair(id, std::make_unique<Buffer>(gfx, StructuredBufferDesc<ViewSpacePositionRadius>(MAX_PARTICLES))));
	}

	void ParticleRenderer::OnEmitterRemoved(size_t id)
	{
		gfx->AddToReleaseQueue(dead_list_buffer_map[id]->Detach());
		gfx->AddToReleaseQueue(alive_index_buffer_map[id]->Detach());
		gfx->AddToReleaseQueue(dead_list_buffer_counter_map[id]->Detach());
		gfx->AddToReleaseQueue(alive_index_buffer_counter_map[id]->Detach());
		gfx->AddToReleaseQueue(particle_bufferA_map[id]->Detach());
		gfx->AddToReleaseQueue(particle_bufferB_map[id]->Detach());
		gfx->AddToReleaseQueue(view_space_positions_buffer_map[id]->Detach());

		dead_list_buffer_map.erase(id);
		alive_index_buffer_map.erase(id);
		dead_list_buffer_counter_map.erase(id);
		alive_index_buffer_counter_map.erase(id);
		particle_bufferA_map.erase(id);
		particle_bufferB_map.erase(id);
		view_space_positions_buffer_map.erase(id);
	}

	void ParticleRenderer::AddInitializeDeadListPass(RenderGraph& rg, Emitter const& emitter_params, size_t id)
	{
		struct ResetDeadListBufferCounterPassData
		{
			RGBufferCopyDstId copy_dst;
		};
		rg.AddPass<ResetDeadListBufferCounterPassData>("Particle Reset Dead List Counter Pass",
			[=](ResetDeadListBufferCounterPassData& data, RenderGraphBuilder& builder)
			{
				data.copy_dst = builder.WriteCopyDstBuffer(RG_RES_NAME_IDX(DeadListBufferCounter, id));
			},
			[=](ResetDeadListBufferCounterPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Buffer const& counter_buffer = context.GetCopyDstBuffer(data.copy_dst);
				cmd_list->CopyBufferRegion(counter_buffer.GetNative(), 0, counter_reset_buffer->GetNative(), 0, sizeof(uint32));
			}, ERGPassType::Copy, ERGPassFlags::None);

		struct InitializeDeadListPassData
		{
			RGBufferReadWriteId dead_list;
		};
		rg.AddPass<InitializeDeadListPassData>("Particle Initialize Dead List Pass",
			[=](InitializeDeadListPassData& data, RenderGraphBuilder& builder)
			{
				data.dead_list = builder.WriteBuffer(RG_RES_NAME_IDX(DeadListBuffer, id), RG_RES_NAME_IDX(DeadListBufferCounter, id));
			},
			[=](InitializeDeadListPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_InitDeadList));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_InitDeadList));
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, descriptor, context.GetReadWriteBuffer(data.dead_list),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(0, descriptor);
				cmd_list->Dispatch((uint32)std::ceil(MAX_PARTICLES * 1.0f / 256), 1, 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

	void ParticleRenderer::AddResetParticlesPass(RenderGraph& rg, Emitter const& emitter_params, size_t id)
	{
		struct InitializeDeadListPassData
		{
			RGBufferReadWriteId particle_bufferA;
			RGBufferReadWriteId particle_bufferB;
		};
		rg.AddPass<InitializeDeadListPassData>("Reset Particles Pass",
			[=](InitializeDeadListPassData& data, RenderGraphBuilder& builder)
			{
				data.particle_bufferA = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleBufferA, id));
				data.particle_bufferB = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleBufferB, id));
			},
			[=](InitializeDeadListPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_Reset));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_Reset));
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, descriptor, context.GetReadWriteBuffer(data.particle_bufferA),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), context.GetReadWriteBuffer(data.particle_bufferB),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(0, descriptor);
				cmd_list->Dispatch((uint32)std::ceil(MAX_PARTICLES * 1.0f / 256), 1, 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

	}

	void ParticleRenderer::AddEmitPass(RenderGraph& rg, Emitter const& emitter_params, size_t id)
	{
		struct CopyCounterPassData
		{
			RGBufferCopyDstId dst;
			RGBufferCopySrcId src;
		};
		rg.AddPass<CopyCounterPassData>("Copy Dead List Counter Pass",
			[=](CopyCounterPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc desc{};
				desc.misc_flags = EBufferMiscFlag::ConstantBuffer;
				desc.size = sizeof(uint32);

				builder.DeclareBuffer(RG_RES_NAME_IDX(DeadListCounterCopy, id), desc);
				data.dst = builder.WriteCopyDstBuffer(RG_RES_NAME_IDX(DeadListCounterCopy, id));
				data.src = builder.ReadCopySrcBuffer(RG_RES_NAME_IDX(DeadListBufferCounter, id));
			},
			[](CopyCounterPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				cmd_list->CopyBufferRegion(ctx.GetCopyDstBuffer(data.dst).GetNative(), 0, ctx.GetCopySrcBuffer(data.src).GetNative(), 0, sizeof(uint32));
			}, ERGPassType::Copy, ERGPassFlags::None);

		struct InitializeDeadListPassData
		{
			RGAllocationId emitter_allocation_id;
			RGBufferReadWriteId particle_bufferA;
			RGBufferReadWriteId particle_bufferB;
			RGBufferReadWriteId dead_list;
			RGBufferConstantId dead_list_counter_copy;
		};

		rg.AddPass<InitializeDeadListPassData>("Particle Emit Pass",
			[=](InitializeDeadListPassData& data, RenderGraphBuilder& builder)
			{
				builder.DeclareAllocation(RG_RES_NAME_IDX(EmitterAllocation, id), AllocDesc{ GetCBufferSize<EmitterCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT });
				data.emitter_allocation_id = builder.UseAllocation(RG_RES_NAME_IDX(EmitterAllocation, id));
				data.particle_bufferA = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleBufferA, id));
				data.particle_bufferB = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleBufferB, id));
				data.dead_list = builder.WriteBuffer(RG_RES_NAME_IDX(DeadListBuffer, id), RG_RES_NAME_IDX(DeadListBufferCounter, id));
				data.dead_list_counter_copy = builder.ReadConstantBuffer(RG_RES_NAME_IDX(DeadListBufferCounter, id));
			},
			[=](InitializeDeadListPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				LinearDynamicAllocator* dynamic_allocator = gfx->GetDynamicAllocator();

				if (emitter_params.number_to_emit > 0)
				{
					EmitterCBuffer emitter_cbuffer_data{};
					emitter_cbuffer_data.ElapsedTime = emitter_params.elapsed_time;
					emitter_cbuffer_data.EmitterPosition = emitter_params.position;
					emitter_cbuffer_data.EmitterVelocity = emitter_params.velocity;
					emitter_cbuffer_data.StartSize = emitter_params.start_size;
					emitter_cbuffer_data.EndSize = emitter_params.end_size;
					emitter_cbuffer_data.Mass = emitter_params.mass;
					emitter_cbuffer_data.MaxParticlesThisFrame = emitter_params.number_to_emit;
					emitter_cbuffer_data.ParticleLifeSpan = emitter_params.particle_lifespan;
					emitter_cbuffer_data.PositionVariance = emitter_params.position_variance;
					emitter_cbuffer_data.VelocityVariance = emitter_params.velocity_variance;
					emitter_cbuffer_data.Collisions = emitter_params.collisions_enabled;
					emitter_cbuffer_data.CollisionThickness = emitter_params.collision_thickness;

					DynamicAllocation emitter_allocation = context.GetAllocation(data.emitter_allocation_id);
					emitter_allocation.Update(emitter_cbuffer_data);

					cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_Emit));
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_Emit));

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
					auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, descriptor, context.GetReadWriteBuffer(data.particle_bufferA),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), context.GetReadWriteBuffer(data.particle_bufferB),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 2), context.GetReadWriteBuffer(data.dead_list),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(0, descriptor);

					descriptor_index = descriptor_allocator->Allocate();
					descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, descriptor, random_texture->GetSubresource_SRV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor);

					Buffer const& buffer = context.GetBuffer(data.dead_list_counter_copy);
					cmd_list->SetComputeRootConstantBufferView(2, buffer.GetGPUAddress()); //constant buffer, add read as constant buffer to builder?
					
					cmd_list->SetComputeRootConstantBufferView(3, emitter_allocation.gpu_address);

					uint32 thread_groups_x = (UINT)std::ceil(emitter_params.number_to_emit * 1.0f / 1024);
					cmd_list->Dispatch(thread_groups_x, 1, 1);
				}
			}, ERGPassType::Compute, ERGPassFlags::None);
	}

	void ParticleRenderer::AddSimulatePass(RenderGraph& rg, Emitter const& emitter_params, size_t id)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct ResetIndexBufferPassData
		{
			RGBufferCopyDstId copy_dst;
		};
		rg.AddPass<ResetIndexBufferPassData>("Particle Reset Index Buffer Pass",
			[=](ResetIndexBufferPassData& data, RenderGraphBuilder& builder)
			{
				data.copy_dst = builder.WriteCopyDstBuffer(RG_RES_NAME_IDX(AliveIndexBufferCounter, id));
			},
			[=](ResetIndexBufferPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Buffer const& counter_buffer = context.GetCopyDstBuffer(data.copy_dst);
				cmd_list->CopyBufferRegion(counter_buffer.GetNative(), 0, counter_reset_buffer->GetNative(), 0, sizeof(uint32));
			}, ERGPassType::Copy, ERGPassFlags::None);

		struct InitializeDeadListPassData
		{
			RGBufferReadWriteId particle_bufferA;
			RGBufferReadWriteId particle_bufferB;
			RGBufferReadWriteId dead_list;
			RGBufferReadWriteId alive_index;
			RGBufferReadWriteId vs_positions;
			RGBufferReadWriteId indirect_render_args;
			RGTextureReadOnlyId depth_srv;
			RGAllocationId alloc_id;
		};
		rg.AddPass<InitializeDeadListPassData>("Particle Simulate Pass",
			[=](InitializeDeadListPassData& data, RenderGraphBuilder& builder)
			{
				data.depth_srv = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				data.alloc_id = builder.UseAllocation(RG_RES_NAME_IDX(EmitterAllocation, id));
				data.particle_bufferA = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleBufferA, id));
				data.particle_bufferB = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleBufferB, id));
				data.dead_list = builder.WriteBuffer(RG_RES_NAME_IDX(DeadListBuffer, id), RG_RES_NAME_IDX(DeadListBufferCounter, id));
				data.alive_index = builder.WriteBuffer(RG_RES_NAME_IDX(AliveIndexBuffer, id), RG_RES_NAME_IDX(AliveIndexBufferCounter, id));
				data.vs_positions = builder.WriteBuffer(RG_RES_NAME_IDX(VSPositions, id));

				RGBufferDesc indirect_render_args_desc{};
				indirect_render_args_desc.size = 5 * sizeof(uint32);
				indirect_render_args_desc.misc_flags = EBufferMiscFlag::IndirectArgs;

				builder.DeclareBuffer(RG_RES_NAME_IDX(ParticleIndirectRenderBuffer, id), indirect_render_args_desc);
				data.indirect_render_args = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleIndirectRenderBuffer, id));
			},
			[=](InitializeDeadListPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_Simulate));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_Simulate));

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(6);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadWriteBuffer(data.particle_bufferA),
															 context.GetReadWriteBuffer(data.particle_bufferB),
															 context.GetReadWriteBuffer(data.dead_list),
															 context.GetReadWriteBuffer(data.alive_index),
															 context.GetReadWriteBuffer(data.vs_positions),
															 context.GetReadWriteBuffer(data.indirect_render_args),
														   };
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor };
				uint32 src_range_sizes[] = { 1, 1, 1, 1, 1, 1 };
				uint32 dst_range_sizes[] = { 6 };
				device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(0, descriptor);

				descriptor_index = descriptor_allocator->Allocate();
				descriptor = descriptor_allocator->GetHandle(descriptor_index);

				device->CopyDescriptorsSimple(1, descriptor, context.GetReadOnlyTexture(data.depth_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor);
				cmd_list->SetComputeRootConstantBufferView(2, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(3, global_data.compute_cbuffer_address);
				cmd_list->SetComputeRootConstantBufferView(4, context.GetAllocation(data.alloc_id).gpu_address);

				uint32 thread_groups_x = (UINT)std::ceil(MAX_PARTICLES * 1.0f / 256);
				cmd_list->Dispatch(thread_groups_x, 1, 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

	}

	void ParticleRenderer::AddSortPasses(RenderGraph& rg, Emitter const& emitter_params, size_t id)
	{
		struct InitializeSortDispatchArgsPassData
		{
			RGBufferReadWriteId indirect_render_args;
			RGBufferConstantId  alive_index_count;
		};
		rg.AddPass<InitializeSortDispatchArgsPassData>("Particle Initialize Sort Dispatch Args Pass",
			[=](InitializeSortDispatchArgsPassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc indirect_sort_args_desc{};
				indirect_sort_args_desc.size = 4 * sizeof(uint32);
				indirect_sort_args_desc.misc_flags = EBufferMiscFlag::IndirectArgs;

				builder.DeclareBuffer(RG_RES_NAME_IDX(ParticleIndirectSortBuffer, id), indirect_sort_args_desc);
				data.indirect_render_args = builder.WriteBuffer(RG_RES_NAME_IDX(ParticleIndirectSortBuffer, id));
				data.alive_index_count = builder.ReadConstantBuffer(RG_RES_NAME_IDX(AliveIndexBufferCounter, id));
			},
			[=](InitializeSortDispatchArgsPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_InitSortDispatchArgs));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_InitSortDispatchArgs));

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);

				device->CopyDescriptorsSimple(1, descriptor, ctx.GetReadWriteBuffer(data.indirect_render_args),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(0, descriptor);
				cmd_list->SetComputeRootConstantBufferView(1, ctx.GetConstantBuffer(data.alive_index_count).GetGPUAddress());
				cmd_list->Dispatch(1, 1, 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		struct InitialSortPassData
		{
			RGBufferConstantId  alive_index_count;
			RGBufferReadWriteId alive_index;
			RGBufferIndirectArgsId indirect_args;
		};
		rg.AddPass<InitialSortPassData>("Particle Initial Sort Pass",
			[=](InitialSortPassData& data, RenderGraphBuilder& builder)
			{
				data.alive_index = builder.WriteBuffer(RG_RES_NAME_IDX(AliveIndexBuffer, id));
				data.alive_index_count = builder.ReadConstantBuffer(RG_RES_NAME_IDX(AliveIndexBufferCounter, id));
				data.indirect_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME_IDX(ParticleIndirectSortBuffer, id));
			},
			[=](InitialSortPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				LinearDynamicAllocator* dynamic_allocator = gfx->GetDynamicAllocator();

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, descriptor, ctx.GetReadWriteBuffer(data.alive_index),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				DynamicAllocation sort_dispatch_info_allocation = dynamic_allocator->Allocate(GetCBufferSize<SortDispatchInfo>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				sort_dispatch_info_allocation.Update(SortDispatchInfo{});
				cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_Sort));
				cmd_list->SetComputeRootDescriptorTable(0, descriptor);
				cmd_list->SetComputeRootConstantBufferView(1, ctx.GetConstantBuffer(data.alive_index_count).GetGPUAddress());
				cmd_list->SetComputeRootConstantBufferView(2, sort_dispatch_info_allocation.gpu_address);

				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_Sort512));
				cmd_list->ExecuteIndirect(indirect_sort_args_signature.Get(), 1, ctx.GetIndirectArgsBuffer(data.indirect_args).GetNative(), 0, nullptr, 0);
				
				D3D12_RESOURCE_BARRIER uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(ctx.GetBuffer(data.alive_index.GetResourceId()).GetNative());
				cmd_list->ResourceBarrier(1, &uav_barrier);

			}, ERGPassType::Compute, ERGPassFlags::None);

		bool needs_incremental_sort = (((MAX_PARTICLES - 1) >> 9) + 1) > 1;
		uint32 presorted = 512;
		while (needs_incremental_sort)
		{
			if (MAX_PARTICLES > presorted * 2) needs_incremental_sort = false;

			struct IncrementalSortPassData
			{
				RGBufferConstantId  alive_index_count;
				RGBufferReadWriteId alive_index;
				RGBufferIndirectArgsId indirect_args;
			};
			rg.AddPass<IncrementalSortPassData>("Particle Incremental Sort Pass",
				[=](IncrementalSortPassData& data, RenderGraphBuilder& builder)
				{
					data.alive_index = builder.WriteBuffer(RG_RES_NAME_IDX(AliveIndexBuffer, id));
					data.alive_index_count = builder.ReadConstantBuffer(RG_RES_NAME_IDX(AliveIndexBufferCounter, id));
					data.indirect_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME_IDX(ParticleIndirectSortBuffer, id));
				},
				[=](IncrementalSortPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					ID3D12Device* device = gfx->GetDevice();
					RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
					LinearDynamicAllocator* dynamic_allocator = gfx->GetDynamicAllocator();

					OffsetType descriptor_index = descriptor_allocator->Allocate();
					auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, descriptor, ctx.GetReadWriteBuffer(data.alive_index),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_Sort));
					cmd_list->SetComputeRootDescriptorTable(0, descriptor);
					cmd_list->SetComputeRootConstantBufferView(1, ctx.GetConstantBuffer(data.alive_index_count).GetGPUAddress());
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_BitonicSortStep));

					uint32 num_thread_groups = 0;
					if (MAX_PARTICLES > presorted)
					{
						uint32 pow2 = presorted;
						while (pow2 < MAX_PARTICLES) pow2 *= 2;
						num_thread_groups = pow2 >> 9;
					}

					uint32 merge_size = presorted * 2;
					for (uint32 merge_subsize = merge_size >> 1; merge_subsize > 256; merge_subsize = merge_subsize >> 1)
					{
						SortDispatchInfo sort_dispatch_info{};
						sort_dispatch_info.x = merge_subsize;
						if (merge_subsize == merge_size >> 1)
						{
							sort_dispatch_info.y = (2 * merge_subsize - 1);
							sort_dispatch_info.z = -1;
						}
						else
						{
							sort_dispatch_info.y = merge_subsize;
							sort_dispatch_info.z = 1;
						}
						sort_dispatch_info.w = 0;

						DynamicAllocation sort_dispatch_info_allocation = dynamic_allocator->Allocate(GetCBufferSize<SortDispatchInfo>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						sort_dispatch_info_allocation.Update(sort_dispatch_info);
						cmd_list->SetComputeRootConstantBufferView(2, sort_dispatch_info_allocation.gpu_address);
						cmd_list->Dispatch(num_thread_groups, 1, 1);

						D3D12_RESOURCE_BARRIER uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(ctx.GetBuffer(data.alive_index.GetResourceId()).GetNative());
						cmd_list->ResourceBarrier(1, &uav_barrier);
					}
					cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_SortInner512));
					cmd_list->Dispatch(num_thread_groups, 1, 1);

				}, ERGPassType::Compute, ERGPassFlags::None);

			presorted *= 2;
		}

	}

	void ParticleRenderer::AddRasterizePass(RenderGraph& rg, Emitter const& emitter_params, size_t id)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct ParticleDrawPass
		{
			RGBufferReadOnlyId particle_bufferA;
			RGBufferReadOnlyId particle_bufferB;
			RGBufferReadOnlyId dead_list;
			RGBufferReadOnlyId alive_index;
			RGBufferReadOnlyId vs_positions;
			RGBufferIndirectArgsId indirect_render_args;
			RGTextureReadOnlyId depth;
			RGBufferConstantId alive_index_counter;
		};

		rg.AddPass<ParticleDrawPass>("Particle Draw Pass",
			[=](ParticleDrawPass& data, RenderGraphBuilder& builder)
			{
				data.particle_bufferA = builder.ReadBuffer(RG_RES_NAME_IDX(ParticleBufferA, id), ReadAccess_PixelShader);
				data.particle_bufferB = builder.ReadBuffer(RG_RES_NAME_IDX(ParticleBufferB, id), ReadAccess_PixelShader);
				data.dead_list = builder.ReadBuffer(RG_RES_NAME_IDX(DeadListBuffer, id), ReadAccess_PixelShader);
				data.alive_index = builder.ReadBuffer(RG_RES_NAME_IDX(AliveIndexBuffer, id), ReadAccess_PixelShader);
				data.vs_positions = builder.ReadBuffer(RG_RES_NAME_IDX(VSPositions, id), ReadAccess_PixelShader);
				data.alive_index_counter = builder.ReadConstantBuffer(RG_RES_NAME_IDX(AliveIndexBufferCounter, id));
				data.indirect_render_args = builder.ReadIndirectArgsBuffer(RG_RES_NAME_IDX(ParticleIndirectRenderBuffer, id));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](ParticleDrawPass const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Particles_Shading));
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Particles_Shading));

				OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
				auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
				Descriptor src_ranges1[] = { ctx.GetReadOnlyBuffer(data.particle_bufferA), ctx.GetReadOnlyBuffer(data.vs_positions), ctx.GetReadOnlyBuffer(data.alive_index) };
				Descriptor dst_ranges1[] = { descriptor };
				uint32 src_range_sizes1[] = { 1, 1, 1 };
				uint32 dst_range_sizes1[] = { 3 };
				device->CopyDescriptors(ARRAYSIZE(dst_ranges1), dst_ranges1, dst_range_sizes1, ARRAYSIZE(src_ranges1), src_ranges1, src_range_sizes1,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(0, descriptor);

				descriptor_index = descriptor_allocator->AllocateRange(2);
				descriptor = descriptor_allocator->GetHandle(descriptor_index);
				Descriptor particle_texture = texture_manager.CpuDescriptorHandle(emitter_params.particle_texture);
				Descriptor depth_texture = ctx.GetReadOnlyTexture(data.depth);
				Descriptor src_ranges2[] = { particle_texture, depth_texture };
				Descriptor dst_ranges2[] = { descriptor };
				uint32 src_range_sizes2[] = { 1, 1 };
				uint32 dst_range_sizes2[] = { 2 };
				device->CopyDescriptors(ARRAYSIZE(dst_ranges2), dst_ranges2, dst_range_sizes2, ARRAYSIZE(src_ranges2), src_ranges2, src_range_sizes2,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(1, descriptor);

				cmd_list->SetGraphicsRootConstantBufferView(2, ctx.GetConstantBuffer(data.alive_index_counter).GetGPUAddress());
				cmd_list->SetGraphicsRootConstantBufferView(3, global_data.frame_cbuffer_address);

				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				cmd_list->IASetVertexBuffers(0, 0, nullptr);
				BindIndexBuffer(cmd_list, index_buffer.get());
				cmd_list->ExecuteIndirect(indirect_render_args_signature.Get(), 1, ctx.GetIndirectArgsBuffer(data.indirect_render_args).GetNative(), 0, nullptr, 0);
			}, ERGPassType::Graphics, ERGPassFlags::None);
	}

}

