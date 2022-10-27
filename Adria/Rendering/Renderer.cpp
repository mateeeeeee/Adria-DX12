#include "Renderer.h"
#include "BlackboardData.h"
#include "Camera.h"
#include "Components.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "ShaderCache.h"
#include "SkyModel.h"
#include "entt/entity/registry.hpp"
#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Utilities/Random.h"

#include "../Utilities/hwbp.h"

using namespace DirectX;

namespace adria
{
	enum ENullHeapSlot
	{
		NULL_HEAP_SLOT_TEXTURE2D,
		NULL_HEAP_SLOT_TEXTURECUBE,
		NULL_HEAP_SLOT_TEXTURE2DARRAY,
		NULL_HEAP_SLOT_RWTEXTURE2D,
		NULL_HEAP_SIZE
	};

	Renderer::Renderer(entt::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height) : reg(reg), gfx(gfx), resource_pool(gfx), 
		texture_manager(gfx, 1000), camera(nullptr), width(width), height(height), 
		backbuffer_count(gfx->BackbufferCount()), backbuffer_index(gfx->BackbufferIndex()), final_texture(nullptr),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count), 
		weather_cbuffer(gfx->GetDevice(), backbuffer_count), compute_cbuffer(gfx->GetDevice(), backbuffer_count),
		new_frame_cbuffer(gfx->GetDevice(), backbuffer_count),
		gbuffer_pass(reg, width, height), ambient_pass(width, height), tonemap_pass(width, height),
		sky_pass(reg, texture_manager, width, height), lighting_pass(width, height), shadow_pass(reg, texture_manager),
		tiled_lighting_pass(reg, width, height) , copy_to_texture_pass(width, height), add_textures_pass(width, height),
		postprocessor(reg, texture_manager, width, height), fxaa_pass(width, height), picking_pass(gfx, width, height),
		clustered_lighting_pass(reg, gfx, width, height), ssao_pass(width, height), hbao_pass(width, height),
		decals_pass(reg, texture_manager, width, height), ocean_renderer(reg, texture_manager, width, height),
		particle_renderer(reg, gfx, texture_manager, width, height), ray_tracer(reg, gfx, width, height), aabb_pass(reg, width, height)
	{
		GPUProfiler::Get().Init(gfx);
		CreateNullHeap();
		CreateSizeDependentResources();
	}

	Renderer::~Renderer()
	{
		GPUProfiler::Get().Destroy();
		gfx->WaitForGPU();
		reg.clear();
	}
	void Renderer::NewFrame(Camera const* _camera)
	{
		ADRIA_ASSERT(_camera);
		camera = _camera;
		backbuffer_index = gfx->BackbufferIndex();
		shadow_pass.SetCamera(camera);
		GPUProfiler::Get().NewFrame();
	}
	void Renderer::Update(float dt)
	{
		UpdateLights();
		UpdatePersistentConstantBuffers(dt);
		CameraFrustumCulling();
		particle_renderer.Update(dt);
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
			global_data.camera_fov = camera->Fov();
			global_data.new_frame_cbuffer_address = new_frame_cbuffer.BufferLocation(backbuffer_index);
			global_data.frame_cbuffer_address = frame_cbuffer.BufferLocation(backbuffer_index);
			global_data.compute_cbuffer_address = compute_cbuffer.BufferLocation(backbuffer_index);
			global_data.weather_cbuffer_address = weather_cbuffer.BufferLocation(backbuffer_index);
			global_data.null_srv_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2D);
			global_data.null_uav_texture2d = null_heap->GetHandle(NULL_HEAP_SLOT_RWTEXTURE2D);
			global_data.null_srv_texture2darray = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURE2DARRAY);
			global_data.null_srv_texturecube = null_heap->GetHandle(NULL_HEAP_SLOT_TEXTURECUBE);
			global_data.white_srv_texture2d = white_default_texture->GetSRV();
			global_data.lights_buffer_gpu_srv = light_array_srv;
			global_data.lights_buffer_cpu_srv = lights_buffer->GetSRV();
		}
		rg_blackboard.Add<GlobalBlackboardData>(std::move(global_data));

		if (update_picking_data)
		{
			picking_data = picking_pass.GetPickingData();
			update_picking_data = false;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE skybox_handle = global_data.null_srv_texturecube;
		if (renderer_settings.postprocess.reflections == EReflections::RTR)
		{
			if (sky_pass.GetSkyType() == ESkyType::Skybox)
			{
				auto skybox_entities = reg.view<Skybox>();
				for (auto e : skybox_entities)
				{
					Skybox skybox = skybox_entities.get<Skybox>(e);
					if (skybox.active && skybox.used_in_rt)
					{
						skybox_handle = texture_manager.GetSRV(skybox.cubemap_texture);
						break;
					}
				}
			}
		}

		gbuffer_pass.AddPass(render_graph);
		decals_pass.AddPass(render_graph);
		switch (renderer_settings.postprocess.ambient_occlusion)
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
			if ((renderer_settings.use_tiled_deferred || renderer_settings.use_clustered_deferred) && !light.casts_shadows) continue;  
			if (light.casts_shadows) shadow_pass.AddPass(render_graph, light, light_id);
			else if (light.ray_traced_shadows) ray_tracer.AddRayTracedShadowsPass(render_graph, light, light_id);
			lighting_pass.AddPass(render_graph, light, light_id);
		}

		if (renderer_settings.use_tiled_deferred)
		{
			tiled_lighting_pass.AddPass(render_graph);
		}
		else if (renderer_settings.use_clustered_deferred)
		{
			clustered_lighting_pass.AddPass(render_graph, true);
		}

		aabb_pass.AddPass(render_graph);
		ocean_renderer.AddPasses(render_graph);
		sky_pass.AddPass(render_graph);
		picking_pass.AddPass(render_graph);
		particle_renderer.AddPasses(render_graph);
		if (renderer_settings.postprocess.reflections == EReflections::RTR)
		{
			ray_tracer.AddRayTracedReflectionsPass(render_graph, skybox_handle);
		}
		postprocessor.AddPasses(render_graph, renderer_settings.postprocess);
		
		render_graph.ImportTexture(RG_RES_NAME(FinalTexture), final_texture.get());
		ResolveToFinalTexture(render_graph);
		if (!renderer_settings.gui_visible) CopyToBackbuffer(render_graph);
		render_graph.Build();
		render_graph.Execute();
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
		sky_pass.OnSceneInitialized(gfx);
		decals_pass.OnSceneInitialized(gfx);
		ssao_pass.OnSceneInitialized(gfx);
		hbao_pass.OnSceneInitialized(gfx);
		postprocessor.OnSceneInitialized(gfx);
		ocean_renderer.OnSceneInitialized(gfx);
		aabb_pass.OnSceneInitialized(gfx);
		particle_renderer.OnSceneInitialized();
		ray_tracer.OnSceneInitialized();

		TextureDesc desc{};
		desc.width = 1;
		desc.height = 1;
		desc.format = EFormat::R32_FLOAT;
		desc.bind_flags = EBindFlag::ShaderResource;
		desc.initial_state = EResourceState::PixelShaderResource;
		desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);

		float v = 1.0f;
		TextureInitialData init_data{};
		init_data.pData = &v;
		init_data.RowPitch = sizeof(float);
		init_data.SlicePitch = 0;
		white_default_texture = std::make_unique<Texture>(gfx, desc, &init_data);
		white_default_texture->CreateSRV();

		texture_manager.OnSceneInitialized();
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
		ldr_desc.format = EFormat::R10G10B10A2_UNORM;
		ldr_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource | EBindFlag::RenderTarget;
		ldr_desc.initial_state = EResourceState::UnorderedAccess;

		final_texture = std::make_unique<Texture>(gfx, ldr_desc);
		final_texture->CreateSRV();
	}

	void Renderer::UpdateLights()
	{
		auto light_view = reg.view<Light>();
		static size_t light_count = 0;
		if (light_count != light_view.size())
		{
			gfx->WaitForGPU();
			light_count = light_view.size();
			lights_buffer = std::make_unique<Buffer>(gfx, StructuredBufferDesc<StructuredLight>(light_count, false, true));
			lights_buffer->CreateSRV();
		}

		std::vector<StructuredLight> structured_lights{};
		structured_lights.reserve(light_view.size());
		for (auto e : light_view)
		{
			StructuredLight structured_light{};
			auto& light = light_view.get<Light>(e);
			structured_light.color = light.color * light.energy;
			structured_light.position = XMVector4Transform(light.position, camera->View());
			structured_light.direction = XMVector4Transform(light.direction, camera->View());
			structured_light.range = light.range;
			structured_light.type = static_cast<int>(light.type);
			structured_light.inner_cosine = light.inner_cosine;
			structured_light.outer_cosine = light.outer_cosine;
			structured_light.active = light.active;
			structured_light.casts_shadows = light.casts_shadows;
			structured_lights.push_back(structured_light);
		}
		lights_buffer->Update(structured_lights.data(), structured_lights.size() * sizeof(StructuredLight));

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		OffsetType i = descriptor_allocator->Allocate();
		auto dst_descriptor = descriptor_allocator->GetHandle(i);
		device->CopyDescriptorsSimple(1, dst_descriptor, lights_buffer->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		light_array_srv = dst_descriptor;
	}

	void Renderer::UpdatePersistentConstantBuffers(float dt)
	{
		static float total_time = 0.0f;
		total_time += dt;

		static NewFrameCBuffer new_frame_cbuf_data{};
		//new frame
		{
			new_frame_cbuf_data.camera_near = camera->Near();
			new_frame_cbuf_data.camera_far = camera->Far();
			new_frame_cbuf_data.camera_position = camera->Position();
			new_frame_cbuf_data.camera_forward = camera->Forward();
			new_frame_cbuf_data.view = camera->View();
			new_frame_cbuf_data.projection = camera->Proj();
			new_frame_cbuf_data.view_projection = camera->ViewProj();
			new_frame_cbuf_data.inverse_view = XMMatrixInverse(nullptr, camera->View());
			new_frame_cbuf_data.inverse_projection = XMMatrixInverse(nullptr, camera->Proj());
			new_frame_cbuf_data.inverse_view_projection = XMMatrixInverse(nullptr, camera->ViewProj());
			new_frame_cbuf_data.reprojection = new_frame_cbuf_data.inverse_view_projection * new_frame_cbuf_data.prev_view_projection;
			new_frame_cbuf_data.screen_resolution_x = (float)width;
			new_frame_cbuf_data.screen_resolution_y = (float)height;
			new_frame_cbuf_data.delta_time = dt;
			new_frame_cbuf_data.total_time = total_time;
			new_frame_cbuf_data.frame_count = gfx->FrameIndex();
			new_frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
			new_frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;
			new_frame_cbuf_data.lights_idx = (int32)light_array_srv.GetHeapOffset();

			new_frame_cbuffer.Update(new_frame_cbuf_data, backbuffer_index);
			new_frame_cbuf_data.prev_view_projection = camera->ViewProj();
		}

		static FrameCBuffer frame_cbuf_data{};
		//frame
		{
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
			frame_cbuf_data.screen_resolution_x = (float)width;
			frame_cbuf_data.screen_resolution_y = (float)height;
			frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
			frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;

			frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);
			frame_cbuf_data.prev_view_projection = camera->ViewProj();
		}

		//weather
		{
			static WeatherCBuffer weather_cbuf_data{};
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
			XMFLOAT3 sky_color(sky_pass.GetSkyColor());
			XMFLOAT2 wind_dir(ocean_renderer.GetWindDirection());

			CloudParameters cloud_params = postprocessor.GetCloudParams();
			weather_cbuf_data.sky_color = XMLoadFloat3(&sky_color);
			weather_cbuf_data.ambient_color = XMVECTOR{ 0.1f, 0.1f, 0.1f, 1.0f };
			weather_cbuf_data.wind_dir = XMVECTOR{ wind_dir.x, 0.0f, wind_dir.y, 0.0f };
			weather_cbuf_data.wind_speed = cloud_params.wind_speed;
			weather_cbuf_data.time = total_time;
			weather_cbuf_data.crispiness = cloud_params.crispiness;
			weather_cbuf_data.curliness = cloud_params.curliness;
			weather_cbuf_data.coverage = cloud_params.coverage;
			weather_cbuf_data.absorption = cloud_params.light_absorption;
			weather_cbuf_data.clouds_bottom_height = cloud_params.clouds_bottom_height;
			weather_cbuf_data.clouds_top_height = cloud_params.clouds_top_height;
			weather_cbuf_data.density_factor = cloud_params.density_factor;
			weather_cbuf_data.cloud_type = cloud_params.cloud_type;

			XMFLOAT3 sun_dir;
			XMStoreFloat3(&sun_dir, XMVector3Normalize(weather_cbuf_data.light_dir));
			SkyParameters sky_params = sky_pass.GetSkyParameters(sun_dir);
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
			BloomParameters bloom_params = postprocessor.GetBloomParams();
			BokehParameters bokeh_params = postprocessor.GetBokehParams();

			std::array<float, 9> coeffs{};
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
			compute_cbuf_data.bloom_scale = bloom_params.bloom_scale;
			compute_cbuf_data.threshold = bloom_params.bloom_threshold;
			compute_cbuf_data.visualize_tiled = tiled_lighting_pass.IsVisualized();
			compute_cbuf_data.visualize_max_lights = tiled_lighting_pass.MaxLightsForVisualization();
			compute_cbuf_data.bokeh_blur_threshold = bokeh_params.bokeh_blur_threshold;
			compute_cbuf_data.bokeh_lum_threshold = bokeh_params.bokeh_lum_threshold;

			compute_cbuf_data.dof_params = XMVectorSet(0, 200, 400, 600); //for now hardcoded
			compute_cbuf_data.bokeh_radius_scale = bokeh_params.bokeh_radius_scale;
			compute_cbuf_data.bokeh_color_scale = bokeh_params.bokeh_color_scale;
			compute_cbuf_data.bokeh_fallout = bokeh_params.bokeh_fallout;

			XMFLOAT2 wind_dir(ocean_renderer.GetWindDirection());
			compute_cbuf_data.ocean_choppiness = ocean_renderer.GetChoppiness();
			compute_cbuf_data.ocean_size = 512;
			compute_cbuf_data.resolution = 512; //fft resolution
			compute_cbuf_data.wind_direction_x = wind_dir.x;
			compute_cbuf_data.wind_direction_y = wind_dir.y;
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

	void Renderer::CopyToBackbuffer(RenderGraph& rg)
	{
		struct CopyToBackbufferPassData
		{
			RGTextureCopyDstId dst;
			RGTextureCopySrcId src;
		};

		rg.AddPass<CopyToBackbufferPassData>("Copy to Backbuffer Pass",
			[=](CopyToBackbufferPassData& data, RenderGraphBuilder& builder)
			{
				data.src = builder.ReadCopySrcTexture(RG_RES_NAME(FinalTexture));
			},
			[=](CopyToBackbufferPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ResourceBarrierBatch barrier;
				barrier.AddTransition(gfx->GetBackbuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
				barrier.Submit(cmd_list);

				Texture const& src_texture = ctx.GetCopySrcTexture(data.src);
				cmd_list->CopyResource(gfx->GetBackbuffer(), src_texture.GetNative());

				barrier.ReverseTransitions();
				barrier.Submit(cmd_list);
			}, ERGPassType::Copy, ERGPassFlags::ForceNoCull);
	}
	void Renderer::ResolveToFinalTexture(RenderGraph& rg)
	{
		RGResourceName final_texture = postprocessor.GetFinalResource();
		if (HasAnyFlag(renderer_settings.postprocess.anti_aliasing, AntiAliasing_FXAA))
		{
			tonemap_pass.AddPass(rg, final_texture, RG_RES_NAME(TonemapOutput));
			fxaa_pass.AddPass(rg, RG_RES_NAME(TonemapOutput));
		}
		else
		{
			tonemap_pass.AddPass(rg, final_texture);
		}
	}
}

