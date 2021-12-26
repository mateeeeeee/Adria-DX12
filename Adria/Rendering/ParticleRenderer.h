#pragma once
#include <unordered_map>
#include <DirectXMath.h>
#include "Enums.h"
#include "Components.h"
#include "../tecs/registry.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/StructuredBuffer.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/GraphicsCoreDX12.h"
#include "../Graphics/DescriptorHeap.h"
#include "../Utilities/Random.h"

namespace adria
{
	//based on AMD GPU Particles Sample: https://github.com/GPUOpen-LibrariesAndSDKs/GPUParticles11

	//make PSO for particle shading and add alpha blend, add uav for indirect buffers

	class ParticleRenderer
	{
		template<typename T>
		using AppendBuffer = StructuredBuffer<T>;

		static constexpr size_t MAX_PARTICLES = 400 * 1024;

		struct GPUParticleA
		{
			DirectX::XMFLOAT4	TintAndAlpha;	// The color and opacity
			f32		Rotation;					// The rotation angle
			u32		IsSleeping;					// Whether or not the particle is sleeping (ie, don't update position)
		};
		struct GPUParticleB
		{
			DirectX::XMFLOAT3	Position;		// World space position
			f32		Mass;						// Mass of particle

			DirectX::XMFLOAT3	Velocity;		// World space velocity
			f32		Lifespan;					// Lifespan of the particle.

			f32		DistanceToEye;				// The distance from the particle to the eye
			f32		Age;						// The current age counting down from lifespan to zero
			f32		StartSize;					// The size at spawn time
			f32		EndSize;					// The time at maximum age
		};
		struct EmitterCBuffer
		{
			DirectX::XMFLOAT4	EmitterPosition;
			DirectX::XMFLOAT4	EmitterVelocity;
			DirectX::XMFLOAT4	PositionVariance;

			i32	MaxParticlesThisFrame;
			f32	ParticleLifeSpan;
			f32	StartSize;
			f32	EndSize;

			f32	VelocityVariance;
			f32	Mass;
			f32	ElapsedTime;
			i32 Collisions;

			i32 CollisionThickness;
		};
		struct IndexBufferElement
		{
			f32	distance;
			f32	index;
		};
		struct ViewSpacePositionRadius
		{
			DirectX::XMFLOAT3 viewspace_position;
			f32 radius;
		};
		struct SortDispatchInfo
		{
			i32 x, y, z, w;
		};
	public:
		ParticleRenderer(GraphicsCoreDX12* gfx) : gfx{ gfx },
			dead_list_buffer(gfx->GetDevice(), MAX_PARTICLES, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			particle_bufferA(gfx->GetDevice(), MAX_PARTICLES),
			particle_bufferB(gfx->GetDevice(), MAX_PARTICLES),
			view_space_positions_buffer(gfx->GetDevice(), MAX_PARTICLES),
			dead_list_count_cbuffer(gfx->GetDevice(), gfx->BackbufferCount()),
			active_list_count_cbuffer(gfx->GetDevice(), gfx->BackbufferCount()),
			emitter_cbuffer(gfx->GetDevice(), gfx->BackbufferCount()),
			sort_dispatch_info_cbuffer(gfx->GetDevice(), gfx->BackbufferCount()),
			alive_index_buffer(gfx->GetDevice(), MAX_PARTICLES, true)
		{
			LoadShaders();
			CreatePipelineStateObjects();
			CreateResources();
		}

		void UploadData()
		{
			ID3D12Device* device = gfx->GetDevice();
			ID3D12GraphicsCommandList* cmd_list = gfx->GetDefaultCommandList();

			ID3D12Resource* particle_upload_texture = nullptr;
			const u64 upload_buffer_size = GetRequiredIntermediateSize(random_texture.Resource(), 0, 1);

			CD3DX12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
			BREAK_IF_FAILED(device->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&particle_upload_texture)));

			RealRandomGenerator rand_float{ 0.0f, 1.0f };
			std::vector<f32> random_texture_data;
			for (u32 i = 0; i < random_texture.Width() * random_texture.Height(); i++)
			{
				random_texture_data.push_back(2.0f * rand_float() - 1.0f);
				random_texture_data.push_back(2.0f * rand_float() - 1.0f);
				random_texture_data.push_back(2.0f * rand_float() - 1.0f);
				random_texture_data.push_back(2.0f * rand_float() - 1.0f);
			}

			D3D12_SUBRESOURCE_DATA data{};
			data.pData = random_texture_data.data();
			data.RowPitch = random_texture.Width() * 4 * sizeof(f32);
			data.SlicePitch = 0;

			UpdateSubresources(cmd_list, random_texture.Resource(), particle_upload_texture, 0, 0, 1, &data);
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(random_texture.Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			cmd_list->ResourceBarrier(1, &barrier);

			gfx->AddToReleaseQueue(particle_upload_texture);

			std::vector<UINT> indices(MAX_PARTICLES * 6);
			UINT base = 0;
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
			index_buffer = std::make_unique<IndexBuffer>(gfx, indices);
		}

		void Update(f32 dt, Emitter& emitter_params)
		{
			emitter_params.elapsed_time += dt;
			if (emitter_params.particles_per_second > 0.0f)
			{
				emitter_params.accumulation += emitter_params.particles_per_second * dt;

				if (emitter_params.accumulation > 1.0f)
				{
					f64 integer_part = 0.0;
					f32 fraction = (f32)modf(emitter_params.accumulation, &integer_part);

					emitter_params.number_to_emit = (i32)integer_part;
					emitter_params.accumulation = fraction;
				}
			}
		}

		void Render(ID3D12GraphicsCommandList* cmd_list, Emitter const& emitter_params, 
			D3D12_CPU_DESCRIPTOR_HANDLE depth_srv, D3D12_CPU_DESCRIPTOR_HANDLE particle_srv)
		{
			if (emitter_params.reset_emitter)
			{
				InitializeDeadList(cmd_list);
				ResetParticles(cmd_list);
				emitter_params.reset_emitter = false;
			}
			Emit(cmd_list, emitter_params);
			Simulate(cmd_list, depth_srv);
			if (emitter_params.sort)
			{
				Sort(cmd_list);
			}
			Rasterize(cmd_list, emitter_params, depth_srv, particle_srv);
		}

	private:
		GraphicsCoreDX12* gfx;

		Texture2D random_texture;
		AppendBuffer<u32> dead_list_buffer;
		StructuredBuffer<GPUParticleA> particle_bufferA;
		StructuredBuffer<GPUParticleB> particle_bufferB;
		StructuredBuffer<ViewSpacePositionRadius> view_space_positions_buffer;
		StructuredBuffer<IndexBufferElement> alive_index_buffer;

		ConstantBuffer<u32> dead_list_count_cbuffer;
		ConstantBuffer<u32> active_list_count_cbuffer;
		ConstantBuffer<EmitterCBuffer> emitter_cbuffer;
		ConstantBuffer<SortDispatchInfo> sort_dispatch_info_cbuffer;

		Microsoft::WRL::ComPtr<ID3D12CommandSignature>  indirect_render_args_signature;
		Microsoft::WRL::ComPtr<ID3D12Resource> indirect_render_args_buffer;
		D3D12_CPU_DESCRIPTOR_HANDLE indirect_render_args_uav;
		Microsoft::WRL::ComPtr<ID3D12CommandSignature>  indirect_sort_args_signature;
		Microsoft::WRL::ComPtr<ID3D12Resource> indirect_sort_args_buffer;
		D3D12_CPU_DESCRIPTOR_HANDLE indirect_sort_args_uav;
		Microsoft::WRL::ComPtr<ID3D12Resource> counter_reset_buffer;

		std::unique_ptr<DescriptorHeap> particle_heap;
		std::unique_ptr<IndexBuffer> index_buffer;

		std::unordered_map<EShader, ShaderBlob> particle_shader_map;
		std::unordered_map<ERootSig, Microsoft::WRL::ComPtr<ID3D12RootSignature>> particle_rs_map;
		std::unordered_map<EPipelineStateObject, Microsoft::WRL::ComPtr<ID3D12PipelineState>> particle_pso_map;

	private:

		void LoadShaders()
		{
			ShaderBlob cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/InitDeadListCS.cso", cs_blob);
			particle_shader_map[CS_ParticleInitDeadList] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleResetCS.cso", cs_blob);
			particle_shader_map[CS_ParticleReset] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleEmitCS.cso", cs_blob);
			particle_shader_map[CS_ParticleEmit] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleSimulateCS.cso", cs_blob);
			particle_shader_map[CS_ParticleSimulate] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BitonicSortStepCS.cso", cs_blob);
			particle_shader_map[CS_ParticleBitonicSortStep] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/Sort512CS.cso", cs_blob);
			particle_shader_map[CS_ParticleSort512] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SortInner512CS.cso", cs_blob);
			particle_shader_map[CS_ParticleSortInner512] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/InitSortDispatchArgsCS.cso", cs_blob);
			particle_shader_map[CS_ParticleInitSortDispatchArgs] = cs_blob;

			ShaderBlob vs_blob, ps_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticleVS.cso", vs_blob);
			particle_shader_map[VS_Particles] = vs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ParticlePS.cso", ps_blob);
			particle_shader_map[PS_Particles] = ps_blob;
		}
		void CreatePipelineStateObjects()
		{
			ID3D12Device* device = gfx->GetDevice();
			D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
				feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

			//root signatures
			{
				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleInitDeadList].GetPointer(), particle_shader_map[CS_ParticleInitDeadList].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_InitDeadList].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleReset].GetPointer(), particle_shader_map[CS_ParticleReset].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_Reset].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleSimulate].GetPointer(), particle_shader_map[CS_ParticleSimulate].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_Simulate].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleEmit].GetPointer(), particle_shader_map[CS_ParticleEmit].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_Emit].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[PS_Particles].GetPointer(), particle_shader_map[PS_Particles].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_Shading].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleInitSortDispatchArgs].GetPointer(), particle_shader_map[CS_ParticleInitSortDispatchArgs].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_InitSortDispatchArgs].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleBitonicSortStep].GetPointer(), particle_shader_map[CS_ParticleBitonicSortStep].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_BitonicSortStep].GetAddressOf())));

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleSort512].GetPointer(), particle_shader_map[CS_ParticleSort512].GetLength(),
					IID_PPV_ARGS(particle_rs_map[ERootSig::Particles_Sort512].GetAddressOf())));
			}

			//PSOs
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_InitDeadList].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleInitDeadList];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_InitDeadList])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_Reset].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleReset];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_Reset])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_Simulate].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleSimulate];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_Simulate])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_Emit].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleEmit];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_Emit])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_InitSortDispatchArgs].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleInitSortDispatchArgs];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_InitSortDispatchArgs])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_BitonicSortStep].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleBitonicSortStep];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_BitonicSortStep])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_Sort512].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleSort512];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_Sort512])));

				pso_desc.CS = particle_shader_map[CS_ParticleSortInner512];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_SortInner512])));
			}
		}
		void CreateResources()
		{
			ID3D12Device* device = gfx->GetDevice();
			particle_heap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 50);

			//creating command signatures
			{
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
			}
			//creating indirect args buffers
			{
				auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

				D3D12_RESOURCE_DESC indirect_render_args_desc{};
				indirect_render_args_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				indirect_render_args_desc.Alignment = 0;
				indirect_render_args_desc.SampleDesc.Count = 1;
				indirect_render_args_desc.Format = DXGI_FORMAT_UNKNOWN;
				indirect_render_args_desc.Width = 5 * sizeof(u32);
				indirect_render_args_desc.Height = 1;
				indirect_render_args_desc.DepthOrArraySize = 1;
				indirect_render_args_desc.MipLevels = 1;
				indirect_render_args_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				indirect_render_args_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

				BREAK_IF_FAILED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &indirect_render_args_desc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&indirect_render_args_buffer)));

				D3D12_RESOURCE_DESC indirect_sort_args_desc{};
				indirect_sort_args_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				indirect_sort_args_desc.Alignment = 0;
				indirect_sort_args_desc.SampleDesc.Count = 1;
				indirect_sort_args_desc.Format = DXGI_FORMAT_UNKNOWN;
				indirect_sort_args_desc.Width = 4 * sizeof(u32);
				indirect_sort_args_desc.Height = 1;
				indirect_sort_args_desc.DepthOrArraySize = 1;
				indirect_sort_args_desc.MipLevels = 1;
				indirect_sort_args_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				indirect_sort_args_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

				BREAK_IF_FAILED(device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &indirect_sort_args_desc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&indirect_sort_args_buffer)));
			}
			//creating random texture
			{
				//noise texture
				texture2d_desc_t noise_desc{};
				noise_desc.width = 1024;
				noise_desc.height = 1024;
				noise_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				noise_desc.start_state = D3D12_RESOURCE_STATE_COPY_DEST;
				noise_desc.clear_value.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				random_texture = Texture2D(device, noise_desc);
			}
			//creating views
			{
				u32 heap_index = 0;
				random_texture.CreateSRV(particle_heap->GetCpuHandle(heap_index++));

				dead_list_buffer.CreateUAV(particle_heap->GetCpuHandle(heap_index++));
				dead_list_buffer.CreateCounterUAV(particle_heap->GetCpuHandle(heap_index++));

				particle_bufferA.CreateSRV(particle_heap->GetCpuHandle(heap_index++));
				particle_bufferA.CreateUAV(particle_heap->GetCpuHandle(heap_index++));

				particle_bufferB.CreateSRV(particle_heap->GetCpuHandle(heap_index++));
				particle_bufferB.CreateUAV(particle_heap->GetCpuHandle(heap_index++));

				view_space_positions_buffer.CreateSRV(particle_heap->GetCpuHandle(heap_index++));
				view_space_positions_buffer.CreateUAV(particle_heap->GetCpuHandle(heap_index++));

				alive_index_buffer.CreateSRV(particle_heap->GetCpuHandle(heap_index++));
				alive_index_buffer.CreateUAV(particle_heap->GetCpuHandle(heap_index++));
				alive_index_buffer.CreateCounterUAV(particle_heap->GetCpuHandle(heap_index++));

				D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uav_desc.Format = DXGI_FORMAT_R32_UINT; 
				uav_desc.Buffer.FirstElement = 0;
				uav_desc.Buffer.NumElements = 5;
				uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

				D3D12_CPU_DESCRIPTOR_HANDLE uav_handle = particle_heap->GetCpuHandle(heap_index++);
				device->CreateUnorderedAccessView(indirect_render_args_buffer.Get(), nullptr, &uav_desc, uav_handle);
				indirect_render_args_uav = uav_handle;

				uav_desc.Buffer.NumElements = 4;
				uav_handle = particle_heap->GetCpuHandle(heap_index++);
				device->CreateUnorderedAccessView(indirect_sort_args_buffer.Get(), nullptr, &uav_desc, uav_handle);
				indirect_sort_args_uav = uav_handle;
			}
			//creating reset counter buffer
			{
				CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(u32));
				CD3DX12_HEAP_PROPERTIES upload_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
				BREAK_IF_FAILED(gfx->GetDevice()->CreateCommittedResource(
					&upload_heap,
					D3D12_HEAP_FLAG_NONE,
					&buffer_desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&counter_reset_buffer)));

				u8* mapped_reset_buffer = nullptr;
				CD3DX12_RANGE read_range(0, 0); 
				BREAK_IF_FAILED(counter_reset_buffer->Map(0, &read_range, reinterpret_cast<void**>(&mapped_reset_buffer)));
				ZeroMemory(mapped_reset_buffer, sizeof(u32));
				counter_reset_buffer->Unmap(0, nullptr);
			}
		}

		void InitializeDeadList(ID3D12GraphicsCommandList* cmd_list)
		{
			ID3D12Device* device = gfx->GetDevice();
			LinearDescriptorAllocator* descriptor_allocator = gfx->GetDescriptorAllocator();

			D3D12_RESOURCE_BARRIER prereset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(dead_list_buffer.CounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
			cmd_list->ResourceBarrier(1, &prereset_barrier);
			cmd_list->CopyBufferRegion(dead_list_buffer.CounterBuffer(), 0, counter_reset_buffer.Get(), 0, sizeof(u32));
			D3D12_RESOURCE_BARRIER postreset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(dead_list_buffer.CounterBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd_list->ResourceBarrier(1, &postreset_barrier);

			cmd_list->SetComputeRootSignature(particle_rs_map[ERootSig::Particles_InitDeadList].Get());
			cmd_list->SetPipelineState(particle_pso_map[EPipelineStateObject::Particles_InitDeadList].Get());
			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), dead_list_buffer.UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch((u32)std::ceil(MAX_PARTICLES * 1.0f / 256), 1, 1);
		}
		void ResetParticles(ID3D12GraphicsCommandList* cmd_list)
		{
			ID3D12Device* device = gfx->GetDevice();
			LinearDescriptorAllocator* descriptor_allocator = gfx->GetDescriptorAllocator();

			cmd_list->SetComputeRootSignature(particle_rs_map[ERootSig::Particles_Reset].Get());
			cmd_list->SetPipelineState(particle_pso_map[EPipelineStateObject::Particles_Reset].Get());
			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), particle_bufferA.UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 1), particle_bufferB.UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch((u32)std::ceil(MAX_PARTICLES * 1.0f / 256), 1, 1);
		}

		void Emit(ID3D12GraphicsCommandList* cmd_list, Emitter const& emitter_params)
		{
			
		}
		void Simulate(ID3D12GraphicsCommandList* cmd_list, D3D12_CPU_DESCRIPTOR_HANDLE depth_srv)
		{

		}

		void Rasterize(ID3D12GraphicsCommandList* cmd_list, Emitter const& emitter_params,
			D3D12_CPU_DESCRIPTOR_HANDLE depth_srv, D3D12_CPU_DESCRIPTOR_HANDLE particle_srv)
		{
			
		}
		
		void Sort(ID3D12GraphicsCommandList* cmd_list)
		{

		}
		bool SortInitial()
		{
			
		}
		bool SortIncremental(u32 presorted)
		{
			
		}
	};
}