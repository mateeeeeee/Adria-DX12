#include "Renderer.h"
#include "GlobalBlackboardData.h"
#include "Camera.h"
#include "Components.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "ShaderManager.h"
#include "SkyModel.h"
#include "entt/entity/registry.hpp"
#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Utilities/Random.h"

using namespace DirectX;

namespace adria
{

	Renderer::Renderer(entt::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height) : reg(reg), gfx(gfx), resource_pool(gfx), 
		texture_manager(gfx, 1000), gpu_profiler(gfx), camera(nullptr), width(width), height(height), 
		backbuffer_count(gfx->BackbufferCount()), backbuffer_index(gfx->BackbufferIndex()), final_texture(nullptr),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count), postprocess_cbuffer(gfx->GetDevice(), backbuffer_count),
		weather_cbuffer(gfx->GetDevice(), backbuffer_count), compute_cbuffer(gfx->GetDevice(), backbuffer_count)
		,gbuffer_pass(reg, gpu_profiler, width, height), ambient_pass(width, height), tonemap_pass(width, height)
		,sky_pass(reg, texture_manager, width, height), lighting_pass(width, height), shadow_pass(reg, texture_manager),
		tiled_lighting_pass(reg, width, height) , copy_to_texture_pass(width, height), add_textures_pass(width, height),
		postprocessor(reg, texture_manager, width, height), fxaa_pass(width, height), picking_pass(gfx, width, height),
		clustered_lighting_pass(reg, gfx, width, height), ssao_pass(width, height), hbao_pass(width, height),
		decals_pass(reg, texture_manager, width, height), ocean_renderer(reg, texture_manager, width, height),
		particle_renderer(reg, gfx, texture_manager, width, height), ray_tracer(reg, gfx, width, height), aabb_pass(reg, width, height)
	{
		CreateNullHeap();
		CreateSizeDependentResources();
	}

	Renderer::~Renderer()
	{
		gfx->WaitForGPU();
		reg.clear();
	}
	void Renderer::NewFrame(Camera const* _camera)
	{
		ADRIA_ASSERT(_camera);
		camera = _camera;
		backbuffer_index = gfx->BackbufferIndex();
		shadow_pass.SetCamera(camera);
	}
	void Renderer::Update(float32 dt)
	{
		UpdatePersistentConstantBuffers(dt);
		CameraFrustumCulling();
		particle_renderer.Update(dt);
		ray_tracer.Update(RayTracingSettings{ .dt = dt, .ao_radius = renderer_settings.postprocessor.rtao_radius});
	}
	void Renderer::Render(RendererSettings const& _settings)
	{
		renderer_settings = _settings;
		RenderGraph render_graph(resource_pool);
		RGBlackboard& rg_blackboard = render_graph.GetBlackboard();

		GlobalBlackboardData global_data{};
		{
			global_data.camera_position = camera->Position();
			global_data.camera_view = camera->View();
			global_data.camera_proj = camera->Proj();
			global_data.camera_viewproj = camera->ViewProj();
			global_data.frame_cbuffer_address = frame_cbuffer.BufferLocation(backbuffer_index);
			global_data.postprocess_cbuffer_address = postprocess_cbuffer.BufferLocation(backbuffer_index);
			global_data.compute_cbuffer_address = compute_cbuffer.BufferLocation(backbuffer_index);
			global_data.weather_cbuffer_address = weather_cbuffer.BufferLocation(backbuffer_index);
			global_data.null_srv_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2D);
			global_data.null_uav_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_RWTEXTURE2D);
			global_data.null_srv_texture2darray = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2DARRAY);
			global_data.null_srv_texturecube = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURECUBE);
		}
		rg_blackboard.Add<GlobalBlackboardData>(std::move(global_data));

		if (update_picking_data)
		{
			picking_data = picking_pass.GetPickingData();
			update_picking_data = false;
		}

		if (renderer_settings.ibl)
		{
			if(!ibl_generated) GenerateIBLTextures();
			render_graph.ImportTexture(RG_RES_NAME(EnvTexture), env_texture.get());
			render_graph.ImportTexture(RG_RES_NAME(IrmapTexture), irmap_texture.get());
			render_graph.ImportTexture(RG_RES_NAME(BrdfTexture), brdf_lut_texture.get());
		}

		gbuffer_pass.AddPass(render_graph, profiler_settings.profile_gbuffer_pass);
		decals_pass.AddPass(render_graph);
		switch (renderer_settings.postprocessor.ambient_occlusion)
		{
		case EAmbientOcclusion::SSAO:
		{
			ssao_pass.AddPass(render_graph);
			break;
		}
		case EAmbientOcclusion::HBAO:
		{
			hbao_pass.AddPass(render_graph);
			break;
		}
		case EAmbientOcclusion::RTAO:
		{
			ray_tracer.AddRayTracedAmbientOcclusionPass(render_graph);
			break;
		}
		case EAmbientOcclusion::None:
		default:
			break;
		}
		ambient_pass.AddPass(render_graph);

		auto light_entities = reg.view<Light>();
		for (entt::entity light_entity : light_entities)
		{
			auto const& light = light_entities.get<Light>(light_entity);
			size_t light_id = entt::to_integral(light_entity);
			if (!light.active) continue;
			if ((renderer_settings.use_tiled_deferred || renderer_settings.use_clustered_deferred) && !light.casts_shadows) continue;  //tiled/clustered deferred takes care of noncasting lights
			
			if (light.casts_shadows) shadow_pass.AddPass(render_graph, light, light_id);
			else if (light.ray_traced_shadows) ray_tracer.AddRayTracedShadowsPass(render_graph, light, light_id);

			lighting_pass.AddPass(render_graph, light, light_id);
		}

		if (renderer_settings.use_tiled_deferred)
		{
			tiled_lighting_pass.AddPass(render_graph);
			if (renderer_settings.visualize_tiled)  add_textures_pass.AddPass(render_graph, RG_RES_NAME(HDR_RenderTarget), RG_RES_NAME(TiledTarget), RG_RES_NAME(TiledDebugTarget), EBlendMode::AlphaBlend);
			else copy_to_texture_pass.AddPass(render_graph, RG_RES_NAME(HDR_RenderTarget), RG_RES_NAME(TiledTarget), EBlendMode::AdditiveBlend);
		}
		else if (renderer_settings.use_clustered_deferred)
		{
			clustered_lighting_pass.AddPass(render_graph, true);
		}

		aabb_pass.AddPass(render_graph);
		ocean_renderer.UpdateOceanColor(renderer_settings.ocean_color);
		ocean_renderer.AddPasses(render_graph, renderer_settings.recreate_initial_spectrum, 
			renderer_settings.ocean_tesselation, renderer_settings.ocean_wireframe);
		sky_pass.AddPass(render_graph, renderer_settings.sky_type);
		picking_pass.AddPass(render_graph);
		particle_renderer.AddPasses(render_graph);

		if (renderer_settings.postprocessor.reflections == EReflections::RTR)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE skybox_handle = global_data.null_srv_texturecube;
			if (renderer_settings.sky_type == ESkyType::Skybox)
			{
				auto skybox_entities = reg.view<Skybox>();
				for (auto e : skybox_entities)
				{
					Skybox skybox = skybox_entities.get<Skybox>(e);
					if (skybox.active && skybox.used_in_rt)
					{
						skybox_handle = texture_manager.CpuDescriptorHandle(skybox.cubemap_texture);
						break;
					}
				}
			}
			ray_tracer.AddRayTracedReflectionsPass(render_graph, skybox_handle);
		}
		postprocessor.AddPasses(render_graph, renderer_settings.postprocessor);

		if (renderer_settings.gui_visible)
		{
			render_graph.ImportTexture(RG_RES_NAME(FinalTexture), final_texture.get());
			ResolveToTexture(render_graph);
		}
		else ResolveToBackbuffer(render_graph);
		
		render_graph.Build();
		render_graph.Execute();
	}

	void Renderer::SetProfilerSettings(ProfilerSettings const& _profiler_settings)
	{
		profiler_settings = _profiler_settings;
	}

	void Renderer::OnResize(uint32 w, uint32 h)
	{
		if (width != w || height != h)
		{
			width = w; height = h;
			CreateSizeDependentResources();
			gbuffer_pass.OnResize(w, h);
			ambient_pass.OnResize(w, h);
			ssao_pass.OnResize(w, h);
			hbao_pass.OnResize(w, h);
			sky_pass.OnResize(w, h);
			lighting_pass.OnResize(w, h);
			tiled_lighting_pass.OnResize(w, h);
			clustered_lighting_pass.OnResize(w, h);
			copy_to_texture_pass.OnResize(w, h);
			tonemap_pass.OnResize(w, h);
			fxaa_pass.OnResize(w, h);
			postprocessor.OnResize(gfx, w, h);
			add_textures_pass.OnResize(w, h);
			picking_pass.OnResize(w, h);
			decals_pass.OnResize(w, h);
			ocean_renderer.OnResize(w, h);
			particle_renderer.OnResize(w, h);
			ray_tracer.OnResize(w, h);
			aabb_pass.OnResize(w, h);
		}
	}
	void Renderer::OnSceneInitialized()
	{
		UINT tex2darray_size = (UINT)texture_manager.handle;
		gfx->ReserveOnlineDescriptors(tex2darray_size);

		ID3D12Device* device = gfx->GetDevice();
		RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		device->CopyDescriptorsSimple(tex2darray_size, descriptor_allocator->GetFirstHandle(),
			texture_manager.texture_srv_heap->GetFirstHandle(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		sky_pass.OnSceneInitialized(gfx);
		decals_pass.OnSceneInitialized(gfx);
		ssao_pass.OnSceneInitialized(gfx);
		hbao_pass.OnSceneInitialized(gfx);
		postprocessor.OnSceneInitialized(gfx);
		ocean_renderer.OnSceneInitialized(gfx);
		aabb_pass.OnSceneInitialized(gfx);
		particle_renderer.OnSceneInitialized();
		ray_tracer.OnSceneInitialized();

		RealRandomGenerator rand_float{ 0.0f, 1.0f };
		for (uint32 i = 0; i < SSAO_KERNEL_SIZE; i++)
		{
			DirectX::XMFLOAT4 _offset = DirectX::XMFLOAT4(2 * rand_float() - 1, 2 * rand_float() - 1, rand_float(), 0.0f);
			DirectX::XMVECTOR offset = DirectX::XMLoadFloat4(&_offset);
			offset = DirectX::XMVector4Normalize(offset);

			offset *= rand_float();
			ssao_kernel[i] = offset;
		}
	}
	void Renderer::OnRightMouseClicked(int32 x, int32 y)
	{
		update_picking_data = true;
	}

	void Renderer::OnParticleEmitterAdded(size_t emitter_id)
	{
		particle_renderer.OnEmitterAdded(emitter_id);
	}

	void Renderer::OnParticleEmitterRemoved(size_t emitter_id)
	{
		particle_renderer.OnEmitterRemoved(emitter_id);
	}

	TextureManager& Renderer::GetTextureManager()
	{
		return texture_manager;
	}

	void Renderer::CreateNullHeap()
	{
		ID3D12Device* device = gfx->GetDevice();
		null_heap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, NULL_HEAP_SIZE);
		D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
		null_srv_desc.Texture2D.MostDetailedMip = 0;
		null_srv_desc.Texture2D.MipLevels = -1;
		null_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		null_srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2D));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURECUBE));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2DARRAY));

		D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc{};
		null_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		null_uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		device->CreateUnorderedAccessView(nullptr, nullptr, &null_uav_desc, null_heap->GetHandle(NULL_HEAP_SLOT_RWTEXTURE2D));
	}
	void Renderer::CreateSizeDependentResources()
	{
		TextureDesc ldr_desc{};
		ldr_desc.width = width;
		ldr_desc.height = height;
		ldr_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
		ldr_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;
		ldr_desc.initial_state = EResourceState::RenderTarget;
		ldr_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);

		final_texture = std::make_unique<Texture>(gfx, ldr_desc);
		final_texture->CreateSubresource_SRV();
	}

	void Renderer::GenerateIBLTextures()
	{
		ID3D12Device* device = gfx->GetDevice();
		auto cmd_list = gfx->GetDefaultCommandList();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		auto skyboxes = reg.view<Skybox>();
		TextureHandle unfiltered_env = INVALID_TEXTURE_HANDLE;
		for (auto skybox : skyboxes)
		{
			auto const& _skybox = skyboxes.get<Skybox>(skybox);
			if (_skybox.active)
			{
				unfiltered_env = _skybox.cubemap_texture;
				break;
			}
		}

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
		{
			const CD3DX12_DESCRIPTOR_RANGE1 descriptor_ranges[] = {
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC},
				{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE},
			};
			CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};
			rootParameters[0].InitAsDescriptorTable(1, &descriptor_ranges[0]);
			rootParameters[1].InitAsDescriptorTable(1, &descriptor_ranges[1]);
			rootParameters[2].InitAsConstants(1, 0);
			CD3DX12_STATIC_SAMPLER_DESC sampler_desc{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR };

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC signature_desc;
			signature_desc.Init_1_1(3, rootParameters, 1, &sampler_desc);
			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			HRESULT hr = D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
			if (error) OutputDebugStringA((char*)error->GetBufferPointer());
			BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
		}

		ID3D12Resource* unfiltered_env_resource = texture_manager.Resource(unfiltered_env);
		ADRIA_ASSERT(unfiltered_env_resource);
		D3D12_RESOURCE_DESC unfiltered_env_desc = unfiltered_env_resource->GetDesc();


		TextureDesc env_desc{};
		env_desc.width = unfiltered_env_desc.Width;
		env_desc.height = unfiltered_env_desc.Height;
		env_desc.array_size = 6;
		env_desc.mip_levels = 0;
		env_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		env_desc.bind_flags = EBindFlag::UnorderedAccess;
		env_desc.initial_state = EResourceState::PixelShaderResource;
		env_desc.misc_flags = ETextureMiscFlag::TextureCube;
		env_texture = std::make_unique<Texture>(gfx, env_desc);
		env_texture->SetName("Env Map");
		env_texture->CreateSubresource_SRV();

		// Compute pre-filtered specular environment map.
		{
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;

			ShaderBlob spmap_shader;
			ShaderCompiler::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SpmapCS.cso", spmap_shader);

			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = root_signature.Get();
			pso_desc.CS = spmap_shader;
			BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));

			ResourceBarrierBatch precopy_barriers{};
			precopy_barriers.AddTransition(env_texture->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			precopy_barriers.AddTransition(unfiltered_env_resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			precopy_barriers.Submit(cmd_list);

			for (uint32 array_slice = 0; array_slice < 6; ++array_slice)
			{
				const uint32 subresource_index = D3D12CalcSubresource(0, array_slice, 0, env_desc.mip_levels, 6);
				const uint32 unfiltered_subresource_index = D3D12CalcSubresource(0, array_slice, 0, unfiltered_env_desc.MipLevels, 6);
				auto dst_copy_region = CD3DX12_TEXTURE_COPY_LOCATION{ env_texture->GetNative(), subresource_index };
				auto src_copy_region = CD3DX12_TEXTURE_COPY_LOCATION{ unfiltered_env_resource, unfiltered_subresource_index };
				cmd_list->CopyTextureRegion(&dst_copy_region, 0, 0, 0, &src_copy_region, nullptr);
			}

			ResourceBarrierBatch postcopy_barriers{};
			postcopy_barriers.AddTransition(env_texture->GetNative(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			postcopy_barriers.AddTransition(unfiltered_env_resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			postcopy_barriers.Submit(cmd_list);

			cmd_list->SetPipelineState(pipeline_state.Get());
			cmd_list->SetComputeRootSignature(root_signature.Get());

			auto unfiltered_env_descriptor = texture_manager.CpuDescriptorHandle(unfiltered_env);
			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), unfiltered_env_descriptor,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

			float delta_roughness = 1.0f / std::max<float32>(float32(env_desc.mip_levels - 1), 1.0f);
			for (uint32 level = 1, size = (uint32)std::max<uint64>(unfiltered_env_desc.Width, unfiltered_env_desc.Height) / 2; level < env_desc.mip_levels; ++level, size /= 2)
			{
				const uint32 num_groups = std::max<uint32>(1, size / 32);
				const float spmap_roughness = level * delta_roughness;

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
				uavDesc.Format = env_texture->GetDesc().format;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.MipSlice = level;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.ArraySize = env_texture->GetDesc().array_size;

				OffsetType descriptor_index = descriptor_allocator->Allocate();

				device->CreateUnorderedAccessView(env_texture->GetNative(), nullptr,
					&uavDesc,
					descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->SetComputeRoot32BitConstants(2, 1, &spmap_roughness, 0);
				cmd_list->Dispatch(num_groups, num_groups, 6);
			}

			auto env_barrier = CD3DX12_RESOURCE_BARRIER::Transition(env_texture->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cmd_list->ResourceBarrier(1, &env_barrier);

			gfx->ExecuteDefaultCommandList();
			gfx->WaitForGPU();
			gfx->ResetDefaultCommandList();
		}

		TextureDesc irmap_desc{};
		irmap_desc.width = 32;
		irmap_desc.height = 32;
		irmap_desc.array_size = 6;
		irmap_desc.mip_levels = 1;
		irmap_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		irmap_desc.bind_flags = EBindFlag::UnorderedAccess;
		irmap_desc.misc_flags = ETextureMiscFlag::TextureCube;
		irmap_desc.initial_state = EResourceState::PixelShaderResource;
		irmap_texture = std::make_unique<Texture>(gfx, irmap_desc);

		irmap_texture->SetName("Irmap");
		// Compute diffuse irradiance cubemap.
		{
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
			ShaderBlob irmap_shader;
			ShaderCompiler::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/IrmapCS.cso", irmap_shader);

			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
			pso_desc.pRootSignature = root_signature.Get();
			pso_desc.CS = irmap_shader;
			BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));

			TextureDesc desc = irmap_texture->GetDesc();
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = desc.format;
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav_desc.Texture2DArray.MipSlice = 0;
			uav_desc.Texture2DArray.FirstArraySlice = 0;
			uav_desc.Texture2DArray.ArraySize = desc.array_size;

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

			device->CreateUnorderedAccessView(irmap_texture->GetNative(), nullptr,
				&uav_desc,
				descriptor_allocator->GetHandle(descriptor_index));

			auto irmap_barrier = CD3DX12_RESOURCE_BARRIER::Transition(irmap_texture->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd_list->ResourceBarrier(1, &irmap_barrier);
			cmd_list->SetPipelineState(pipeline_state.Get());
			cmd_list->SetComputeRootSignature(root_signature.Get());
			ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
			cmd_list->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);

			device->CopyDescriptorsSimple(1,
				descriptor_allocator->GetHandle(descriptor_index + 1), env_texture->GetSubresource_SRV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index + 1));
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
			cmd_list->Dispatch((uint32)desc.width / 32, (uint32)desc.height / 32, 6u);
			irmap_barrier = CD3DX12_RESOURCE_BARRIER::Transition(irmap_texture->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cmd_list->ResourceBarrier(1, &irmap_barrier);

			gfx->ExecuteDefaultCommandList();
			gfx->WaitForGPU();
			gfx->ResetDefaultCommandList();
		}

		// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
		TextureDesc brdf_desc{};
		brdf_desc.width = 256;
		brdf_desc.height = 256;
		brdf_desc.array_size = 1;
		brdf_desc.mip_levels = 1;
		brdf_desc.format = DXGI_FORMAT_R16G16_FLOAT;
		brdf_desc.bind_flags = EBindFlag::UnorderedAccess;
		brdf_desc.initial_state = EResourceState::PixelShaderResource;
		brdf_lut_texture = std::make_unique<Texture>(gfx, brdf_desc);

		brdf_lut_texture->SetName("BrdfLut");
		{
			TextureDesc desc = brdf_lut_texture->GetDesc();

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			uav_desc.Format = desc.format;
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uav_desc.Texture2DArray.MipSlice = 0;
			uav_desc.Texture2DArray.FirstArraySlice = 0;
			uav_desc.Texture2DArray.ArraySize = desc.array_size;

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CreateUnorderedAccessView(brdf_lut_texture->GetNative(), nullptr,
				&uav_desc,
				descriptor_allocator->GetHandle(descriptor_index));

			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
			ShaderBlob BRDFShader;
			ShaderCompiler::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SpbrdfCS.cso", BRDFShader);

			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = root_signature.Get();
			pso_desc.CS = BRDFShader;
			BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));

			auto brdf_barrier = CD3DX12_RESOURCE_BARRIER::Transition(brdf_lut_texture->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd_list->ResourceBarrier(1, &brdf_barrier);
			cmd_list->SetPipelineState(pipeline_state.Get());
			cmd_list->SetComputeRootSignature(root_signature.Get());
			ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
			cmd_list->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);

			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
			cmd_list->Dispatch((uint32)desc.width / 32, (uint32)desc.height / 32, 1);

			brdf_barrier = CD3DX12_RESOURCE_BARRIER::Transition(brdf_lut_texture->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cmd_list->ResourceBarrier(1, &brdf_barrier);

			gfx->ExecuteDefaultCommandList();
			gfx->WaitForGPU();
			gfx->ResetDefaultCommandList();
			cmd_list->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);
		}

		ibl_generated = true;
	}

	void Renderer::UpdatePersistentConstantBuffers(float32 dt)
	{
		//frame
		{
			static FrameCBuffer frame_cbuf_data{};
			frame_cbuf_data.global_ambient = XMVectorSet(renderer_settings.ambient_color[0], renderer_settings.ambient_color[1], renderer_settings.ambient_color[2], 1);
			frame_cbuf_data.camera_near = camera->Near();
			frame_cbuf_data.camera_far = camera->Far();
			frame_cbuf_data.camera_position = camera->Position();
			frame_cbuf_data.camera_forward = camera->Forward();
			frame_cbuf_data.view = camera->View();
			frame_cbuf_data.projection = camera->Proj();
			frame_cbuf_data.view_projection = camera->ViewProj();
			frame_cbuf_data.inverse_view = DirectX::XMMatrixInverse(nullptr, camera->View());
			frame_cbuf_data.inverse_projection = DirectX::XMMatrixInverse(nullptr, camera->Proj());
			frame_cbuf_data.inverse_view_projection = DirectX::XMMatrixInverse(nullptr, camera->ViewProj());
			frame_cbuf_data.screen_resolution_x = (float32)width;
			frame_cbuf_data.screen_resolution_y = (float32)height;
			frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
			frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;

			frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);
			frame_cbuf_data.prev_view_projection = camera->ViewProj();
		}
		
		//postprocess
		{
			PostprocessSettings const& settings = renderer_settings.postprocessor;
			static PostprocessCBuffer postprocess_cbuf_data{};
			postprocess_cbuf_data.tone_map_exposure = settings.tonemap_exposure;
			postprocess_cbuf_data.tone_map_operator = static_cast<int>(settings.tone_map_op);
			postprocess_cbuf_data.noise_scale = XMFLOAT2((float32)width / 8, (float32)height / 8);
			postprocess_cbuf_data.ssao_power = settings.ssao_power;
			postprocess_cbuf_data.ssao_radius = settings.ssao_radius;
			for (uint32 i = 0; i < SSAO_KERNEL_SIZE; ++i) postprocess_cbuf_data.samples[i] = ssao_kernel[i];
			postprocess_cbuf_data.ssr_ray_step = settings.ssr_ray_step;
			postprocess_cbuf_data.ssr_ray_hit_threshold = settings.ssr_ray_hit_threshold;
			postprocess_cbuf_data.dof_params = XMVectorSet(settings.dof_near_blur, settings.dof_near, settings.dof_far, settings.dof_far_blur);
			postprocess_cbuf_data.velocity_buffer_scale = settings.velocity_buffer_scale;
			postprocess_cbuf_data.fog_falloff = settings.fog_falloff;
			postprocess_cbuf_data.fog_density = settings.fog_density;
			postprocess_cbuf_data.fog_type = static_cast<int32>(settings.fog_type);
			postprocess_cbuf_data.fog_start = settings.fog_start;
			postprocess_cbuf_data.fog_color = XMVectorSet(settings.fog_color[0], settings.fog_color[1], settings.fog_color[2], 1);
			postprocess_cbuf_data.hbao_r2 = settings.hbao_radius * settings.hbao_radius;
			postprocess_cbuf_data.hbao_radius_to_screen = settings.hbao_radius * 0.5f * float32(height) / (tanf(camera->Fov() * 0.5f) * 2.0f);
			postprocess_cbuf_data.hbao_power = settings.hbao_power;
			postprocess_cbuffer.Update(postprocess_cbuf_data, backbuffer_index);

		}
		
		//weather
		{
			static WeatherCBuffer weather_cbuf_data{};
			static float32 total_time = 0.0f;
			total_time += dt;

			auto lights = reg.view<Light>();
			for (auto light : lights)
			{
				auto const& light_data = lights.get<Light>(light);
				if (light_data.type == ELightType::Directional && light_data.active)
				{
					weather_cbuf_data.light_dir = XMVector3Normalize(-light_data.direction);
					weather_cbuf_data.light_color = light_data.color * light_data.energy;
					break;
				}
			}

			weather_cbuf_data.sky_color = XMVECTOR{ renderer_settings.sky_color[0], renderer_settings.sky_color[1],renderer_settings.sky_color[2], 1.0f };
			weather_cbuf_data.ambient_color = XMVECTOR{ renderer_settings.ambient_color[0], renderer_settings.ambient_color[1], renderer_settings.ambient_color[2], 1.0f };
			weather_cbuf_data.wind_dir = XMVECTOR{ renderer_settings.wind_direction[0], 0.0f, renderer_settings.wind_direction[1], 0.0f };
			weather_cbuf_data.wind_speed = renderer_settings.postprocessor.wind_speed;
			weather_cbuf_data.time = total_time;
			weather_cbuf_data.crispiness = renderer_settings.postprocessor.crispiness;
			weather_cbuf_data.curliness = renderer_settings.postprocessor.curliness;
			weather_cbuf_data.coverage = renderer_settings.postprocessor.coverage;
			weather_cbuf_data.absorption = renderer_settings.postprocessor.light_absorption;
			weather_cbuf_data.clouds_bottom_height = renderer_settings.postprocessor.clouds_bottom_height;
			weather_cbuf_data.clouds_top_height = renderer_settings.postprocessor.clouds_top_height;
			weather_cbuf_data.density_factor = renderer_settings.postprocessor.density_factor;
			weather_cbuf_data.cloud_type = renderer_settings.postprocessor.cloud_type;

			XMFLOAT3 sun_dir;
			XMStoreFloat3(&sun_dir, XMVector3Normalize(weather_cbuf_data.light_dir));
			SkyParameters sky_params = CalculateSkyParameters(renderer_settings.turbidity, renderer_settings.ground_albedo, sun_dir);

			weather_cbuf_data.A = sky_params[ESkyParam_A];
			weather_cbuf_data.B = sky_params[ESkyParam_B];
			weather_cbuf_data.C = sky_params[ESkyParam_C];
			weather_cbuf_data.D = sky_params[ESkyParam_D];
			weather_cbuf_data.E = sky_params[ESkyParam_E];
			weather_cbuf_data.F = sky_params[ESkyParam_F];
			weather_cbuf_data.G = sky_params[ESkyParam_G];
			weather_cbuf_data.H = sky_params[ESkyParam_H];
			weather_cbuf_data.I = sky_params[ESkyParam_I];
			weather_cbuf_data.Z = sky_params[ESkyParam_Z];

			weather_cbuffer.Update(weather_cbuf_data, backbuffer_index);
		}

		//compute 
		{
			std::array<float32, 9> coeffs{};
			coeffs.fill(1.0f / 9);
			static ComputeCBuffer compute_cbuf_data{};
			compute_cbuf_data.gauss_coeff1 = coeffs[0];
			compute_cbuf_data.gauss_coeff2 = coeffs[1];
			compute_cbuf_data.gauss_coeff3 = coeffs[2];
			compute_cbuf_data.gauss_coeff4 = coeffs[3];
			compute_cbuf_data.gauss_coeff5 = coeffs[4];
			compute_cbuf_data.gauss_coeff6 = coeffs[5];
			compute_cbuf_data.gauss_coeff7 = coeffs[6];
			compute_cbuf_data.gauss_coeff8 = coeffs[7];
			compute_cbuf_data.gauss_coeff9 = coeffs[8];
			compute_cbuf_data.bloom_scale = renderer_settings.postprocessor.bloom_scale;
			compute_cbuf_data.threshold = renderer_settings.postprocessor.bloom_threshold;
			compute_cbuf_data.visualize_tiled = renderer_settings.visualize_tiled;
			compute_cbuf_data.visualize_max_lights = renderer_settings.visualize_max_lights;
			compute_cbuf_data.bokeh_blur_threshold = renderer_settings.postprocessor.bokeh_blur_threshold;
			compute_cbuf_data.bokeh_lum_threshold = renderer_settings.postprocessor.bokeh_lum_threshold;
			compute_cbuf_data.dof_params = XMVectorSet(renderer_settings.postprocessor.dof_near_blur, renderer_settings.postprocessor.dof_near, renderer_settings.postprocessor.dof_far, renderer_settings.postprocessor.dof_far_blur);
			compute_cbuf_data.bokeh_radius_scale = renderer_settings.postprocessor.bokeh_radius_scale;
			compute_cbuf_data.bokeh_color_scale = renderer_settings.postprocessor.bokeh_color_scale;
			compute_cbuf_data.bokeh_fallout = renderer_settings.postprocessor.bokeh_fallout;

			compute_cbuf_data.ocean_choppiness = renderer_settings.ocean_choppiness;
			compute_cbuf_data.ocean_size = 512;
			compute_cbuf_data.resolution = 512; //fft resolution
			compute_cbuf_data.wind_direction_x = renderer_settings.wind_direction[0];
			compute_cbuf_data.wind_direction_y = renderer_settings.wind_direction[1];
			compute_cbuf_data.delta_time = dt;

			compute_cbuffer.Update(compute_cbuf_data, backbuffer_index);
		}
	}
	void Renderer::CameraFrustumCulling()
	{
		BoundingFrustum camera_frustum = camera->Frustum();
		auto aabb_view = reg.view<AABB>();
		for (auto e : aabb_view)
		{
			auto& aabb = aabb_view.get<AABB>(e);
			aabb.camera_visible = camera_frustum.Intersects(aabb.bounding_box) || reg.all_of<Light>(e); //dont cull lights for now
		}
	}

	void Renderer::ResolveToBackbuffer(RenderGraph& rg)
	{
		if (HasAnyFlag(renderer_settings.postprocessor.anti_aliasing, AntiAliasing_FXAA))
		{
			tonemap_pass.AddPass(rg, postprocessor.GetFinalResource(), RG_RES_NAME(FXAAInput));
			fxaa_pass.AddPass(rg, RG_RES_NAME(FXAAInput), true);
		}
		else
		{
			tonemap_pass.AddPass(rg, postprocessor.GetFinalResource(), true);
		}
	}
	void Renderer::ResolveToTexture(RenderGraph& rg)
	{
		if (HasAnyFlag(renderer_settings.postprocessor.anti_aliasing, AntiAliasing_FXAA))
		{
			tonemap_pass.AddPass(rg, postprocessor.GetFinalResource(), RG_RES_NAME(FXAAInput));
			fxaa_pass.AddPass(rg, RG_RES_NAME(FXAAInput), false);
		}
		else
		{
			tonemap_pass.AddPass(rg, postprocessor.GetFinalResource(), false);
		}
	}

}

