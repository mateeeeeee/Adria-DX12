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
#include "../Utilities/Random.h"

namespace adria
{
	//based on AMD GPU Particles Sample: https://github.com/GPUOpen-LibrariesAndSDKs/GPUParticles11

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
			dead_list_buffer(gfx->GetDevice(), MAX_PARTICLES, true),
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
			CreateRandomTexture();
			CreateIndexBuffer();
			CreateIndirectArgsBuffers();
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

		void Render(Emitter const& emitter_params,
			D3D12_GPU_DESCRIPTOR_HANDLE depth_srv,
			D3D12_GPU_DESCRIPTOR_HANDLE particle_srv)
		{
			if (emitter_params.reset_emitter)
			{
				InitializeDeadList();
				ResetParticles();
				emitter_params.reset_emitter = false;
			}
			Emit(emitter_params);
			Simulate(depth_srv);
			if (emitter_params.sort)
			{
				Sort();
			}
			Rasterize(emitter_params, depth_srv, particle_srv);
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
		//uav needed

		Microsoft::WRL::ComPtr<ID3D12CommandSignature>  indirect_sort_args_signature;
		Microsoft::WRL::ComPtr<ID3D12Resource> indirect_sort_args_buffer;
		//uav needed

		Microsoft::WRL::ComPtr<ID3D12Resource> counter_reset_buffer;

		std::unique_ptr<IndexBuffer> index_buffer;

		//add descriptor heap
		
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
			particle_shader_map[CS_ParticleSortInitArgs] = cs_blob;

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

				BREAK_IF_FAILED(device->CreateRootSignature(0, particle_shader_map[CS_ParticleSortInitArgs].GetPointer(), particle_shader_map[CS_ParticleSortInitArgs].GetLength(),
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

				/*pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_InitDeadList].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleInitDeadList];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_InitDeadList])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_InitDeadList].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleInitDeadList];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_InitDeadList])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_InitDeadList].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleInitDeadList];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_InitDeadList])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_InitDeadList].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleInitDeadList];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_InitDeadList])));

				pso_desc.pRootSignature = particle_rs_map[ERootSig::Particles_InitDeadList].Get();
				pso_desc.CS = particle_shader_map[CS_ParticleInitDeadList];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&particle_pso_map[EPipelineStateObject::Particles_InitDeadList])));*/
			}
		}

		void CreateIndirectArgsBuffers()
		{
			
		}
		void CreateIndexBuffer()
		{
			
		}
		void CreateRandomTexture()
		{
			
		}

		void InitializeDeadList()
		{
			
		}
		void ResetParticles()
		{

		}
		void Emit(Emitter const& emitter_params)
		{
			
		}
		void Simulate(D3D12_GPU_DESCRIPTOR_HANDLE depth_srv)
		{

		}
		void Rasterize(Emitter const& emitter_params, D3D12_GPU_DESCRIPTOR_HANDLE depth_srv, D3D12_GPU_DESCRIPTOR_HANDLE particle_srv)
		{

		}
		void Sort()
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