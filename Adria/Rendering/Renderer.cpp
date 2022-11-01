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
	namespace shadows
	{
		static constexpr uint32 SHADOW_MAP_SIZE = 1024;
		static constexpr uint32 SHADOW_CUBE_SIZE = 512;
		static constexpr uint32 SHADOW_CASCADE_MAP_SIZE = 2048;
		static constexpr uint32 SHADOW_CASCADE_COUNT = 4;

		std::array<XMMATRIX, SHADOW_CASCADE_COUNT> RecalculateProjectionMatrices(Camera const& camera, float split_lambda, std::array<float, SHADOW_CASCADE_COUNT>& split_distances)
		{
			float camera_near = camera.Near();
			float camera_far = camera.Far();
			float fov = camera.Fov();
			float ar = camera.AspectRatio();

			float f = 1.0f / SHADOW_CASCADE_COUNT;
			for (uint32 i = 0; i < split_distances.size(); i++)
			{
				float fi = (i + 1) * f;
				float l = camera_near * pow(camera_far / camera_near, fi);
				float u = camera_near + (camera_far - camera_near) * fi;
				split_distances[i] = l * split_lambda + u * (1.0f - split_lambda);
			}

			std::array<XMMATRIX, SHADOW_CASCADE_COUNT> projection_matrices{};
			projection_matrices[0] = XMMatrixPerspectiveFovLH(fov, ar, camera_near, split_distances[0]);
			for (uint32 i = 1; i < projection_matrices.size(); ++i)
				projection_matrices[i] = XMMatrixPerspectiveFovLH(fov, ar, split_distances[i - 1], split_distances[i]);
			return projection_matrices;
		}

		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Directional(Light const& light, Camera const& camera)
		{
			BoundingFrustum frustum = camera.Frustum();
			std::array<XMFLOAT3, frustum.CORNER_COUNT> corners = {};
			frustum.GetCorners(corners.data());

			BoundingSphere frustum_sphere;
			BoundingSphere::CreateFromFrustum(frustum_sphere, frustum);

			XMVECTOR frustum_center = XMVectorSet(0, 0, 0, 0);
			for (uint32 i = 0; i < corners.size(); ++i)
			{
				frustum_center = frustum_center + XMLoadFloat3(&corners[i]);
			}
			frustum_center /= static_cast<float>(corners.size());

			float radius = 0.0f;
			for (uint32 i = 0; i < corners.size(); ++i)
			{
				float dist = DirectX::XMVectorGetX(XMVector3Length(DirectX::XMLoadFloat3(&corners[i]) - frustum_center));
				radius = (std::max)(radius, dist);
			}
			radius = std::ceil(radius * 8.0f) / 8.0f;

			XMVECTOR const max_extents = XMVectorSet(radius, radius, radius, 0);
			XMVECTOR const min_extents = -max_extents;
			XMVECTOR const cascade_extents = max_extents - min_extents;

			XMVECTOR light_dir = XMVector3Normalize(light.direction);
			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMMATRIX V = XMMatrixLookAtLH(frustum_center, frustum_center + 1.0f * light_dir * radius, up);

			XMFLOAT3 min_e, max_e, cascade_e;
			XMStoreFloat3(&min_e, min_extents);
			XMStoreFloat3(&max_e, max_extents);
			XMStoreFloat3(&cascade_e, cascade_extents);

			float l = min_e.x;
			float b = min_e.y;
			float n = min_e.z; 
			float r = max_e.x;
			float t = max_e.y;
			float f = max_e.z * 1.5f;

			XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
			XMMATRIX VP = V * P;
			XMFLOAT3 so(0, 0, 0);
			XMVECTOR shadow_origin = XMLoadFloat3(&so);
			shadow_origin = XMVector3Transform(shadow_origin, VP); //sOrigin * P;
			shadow_origin *= (ShadowPass::SHADOW_MAP_SIZE / 2.0f);

			XMVECTOR rounded_origin = XMVectorRound(shadow_origin);
			XMVECTOR rounded_offset = rounded_origin - shadow_origin;
			rounded_offset *= (2.0f / ShadowPass::SHADOW_MAP_SIZE);
			rounded_offset *= XMVectorSet(1.0, 1.0, 0.0, 0.0);

			P.r[3] += rounded_offset;
			return { V,P };
		}
		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Spot(Light const& light)
		{
			ADRIA_ASSERT(light.type == ELightType::Spot);

			XMVECTOR light_dir = XMVector3Normalize(light.direction);
			XMVECTOR light_pos = light.position;
			XMVECTOR target_pos = light_pos + light_dir * light.range;

			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			XMMATRIX V = XMMatrixLookAtLH(light_pos, target_pos, up);
			static const float shadow_near = 0.5f;
			float fov_angle = 2.0f * acos(light.outer_cosine);
			XMMATRIX P = XMMatrixPerspectiveFovLH(fov_angle, 1.0f, shadow_near, light.range);
			return { V,P };
		}
		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Point(Light const& light, uint32 face_index)
		{
			static float const shadow_near = 0.5f;
			XMMATRIX P = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, shadow_near, light.range);

			XMVECTOR light_pos = light.position;
			XMMATRIX V{};
			XMVECTOR target{};
			XMVECTOR up{};
			switch (face_index)
			{
			case 0: 
				target = XMVectorAdd(light_pos, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 1: 
				target = XMVectorAdd(light_pos, XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 2:	 
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 3:  
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 4: 
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 5:  
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			default:
				ADRIA_ASSERT(false && "Invalid face index!");
			}
			return { V,P };
		}
		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Cascades(Light const& light, Camera const& camera, XMMATRIX projection_matrix)
		{
			static float const far_factor = 1.5f;
			static float const light_distance_factor = 1.0f;

			BoundingFrustum frustum(projection_matrix);
			frustum.Transform(frustum, XMMatrixInverse(nullptr, camera.View()));
			std::array<XMFLOAT3, frustum.CORNER_COUNT> corners{};
			frustum.GetCorners(corners.data());

			XMVECTOR frustum_center = XMVectorSet(0, 0, 0, 0);
			for (uint32 i = 0; i < corners.size(); ++i)
			{
				frustum_center = frustum_center + XMLoadFloat3(&corners[i]);
			}
			frustum_center /= static_cast<float>(corners.size());

			float radius = 0.0f;
			for (uint32 i = 0; i < corners.size(); ++i)
			{
				float dist = XMVectorGetX(XMVector3Length(XMLoadFloat3(&corners[i]) - frustum_center));
				radius = (std::max)(radius, dist);
			}
			radius = std::ceil(radius * 8.0f) / 8.0f;

			XMVECTOR const max_extents = XMVectorSet(radius, radius, radius, 0);
			XMVECTOR const min_extents = -max_extents;
			XMVECTOR const cascade_extents = max_extents - min_extents;

			XMVECTOR light_dir = light.direction;
			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMMATRIX V = XMMatrixLookAtLH(frustum_center, frustum_center + light_distance_factor * light_dir * radius, up);
			XMFLOAT3 min_e, max_e, cascade_e;

			XMStoreFloat3(&min_e, min_extents);
			XMStoreFloat3(&max_e, max_extents);
			XMStoreFloat3(&cascade_e, cascade_extents);

			float l = min_e.x;
			float b = min_e.y;
			float n = min_e.z - far_factor * radius;
			float r = max_e.x;
			float t = max_e.y;
			float f = max_e.z * far_factor;

			XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
			XMMATRIX VP = V * P;
			XMFLOAT3 so(0, 0, 0);

			XMVECTOR shadow_origin = XMLoadFloat3(&so);
			shadow_origin = XMVector3Transform(shadow_origin, VP); 
			shadow_origin *= (SHADOW_CASCADE_MAP_SIZE / 2.0f);

			XMVECTOR rounded_origin = XMVectorRound(shadow_origin);
			XMVECTOR round_offset = rounded_origin - shadow_origin;

			round_offset *= (2.0f / SHADOW_CASCADE_MAP_SIZE);
			round_offset *= XMVectorSet(1.0, 1.0, 0.0, 0.0);
			P.r[3] += round_offset;
			return { V,P };
		}
	}

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
		old_frame_cbuffer(gfx->GetDevice(), backbuffer_count), 
		compute_cbuffer(gfx->GetDevice(), backbuffer_count),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count),
		gbuffer_pass(reg, width, height), ambient_pass(width, height), tonemap_pass(width, height),
		sky_pass(reg, texture_manager, width, height), deferred_lighting_pass(width, height), shadow_pass(reg, texture_manager),
		tiled_lighting_pass(reg, width, height) , copy_to_texture_pass(width, height), add_textures_pass(width, height),
		postprocessor(reg, texture_manager, width, height), fxaa_pass(width, height), picking_pass(gfx, width, height),
		clustered_lighting_pass(reg, gfx, width, height), ssao_pass(width, height), hbao_pass(width, height),
		decals_pass(reg, texture_manager, width, height), ocean_renderer(reg, texture_manager, width, height),
		ray_tracer(reg, gfx, width, height), aabb_pass(reg, width, height)
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
		SetupShadows();
		UpdateLights();
		UpdatePersistentConstantBuffers(dt);
		CameraFrustumCulling();
	}
	void Renderer::Render(RendererSettings const& _settings)
	{
		renderer_settings = _settings;
		texture_manager.Tick();

		RenderGraph render_graph(resource_pool);
		RGBlackboard& rg_blackboard = render_graph.GetBlackboard();
		GlobalBlackboardData global_data{};
		{
			global_data.camera_position = camera->Position();
			global_data.camera_view = camera->View();
			global_data.camera_proj = camera->Proj();
			global_data.camera_viewproj = camera->ViewProj();
			global_data.camera_fov = camera->Fov();
			global_data.new_frame_cbuffer_address = frame_cbuffer.BufferLocation(backbuffer_index);
			global_data.frame_cbuffer_address = old_frame_cbuffer.BufferLocation(backbuffer_index);
			global_data.compute_cbuffer_address = compute_cbuffer.BufferLocation(backbuffer_index);
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
			//if ((renderer_settings.use_tiled_deferred || renderer_settings.use_clustered_deferred) && !light.casts_shadows) continue;  
			if (light.casts_shadows) shadow_pass.AddPass(render_graph, light, light_id);
			else if (light.ray_traced_shadows) ray_tracer.AddRayTracedShadowsPass(render_graph, light, light_id);
			deferred_lighting_pass.AddPass(render_graph, light, light_id);
		}

		//if (renderer_settings.use_tiled_deferred)
		//{
		//	tiled_lighting_pass.AddPass(render_graph);
		//}
		//else if (renderer_settings.use_clustered_deferred)
		//{
		//	clustered_lighting_pass.AddPass(render_graph, true);
		//}

		aabb_pass.AddPass(render_graph);
		ocean_renderer.AddPasses(render_graph);
		sky_pass.AddPass(render_graph, sun_direction);
		picking_pass.AddPass(render_graph);
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
			deferred_lighting_pass.OnResize(w, h);
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

	void Renderer::SetupShadows()
	{
		static size_t light_projections_count = 0;
		size_t current_light_projections_count = 0;

		auto light_view = reg.view<Light>();
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			if (light.casts_shadows)
			{
				if (light.type == ELightType::Directional && light.use_cascades) current_light_projections_count += shadows::SHADOW_CASCADE_COUNT;
				else if (light.type == ELightType::Point) current_light_projections_count += 6;
				else current_light_projections_count++;
			}
		}
		if (current_light_projections_count != light_projections_count)
		{
			gfx->WaitForGPU();
			light_projections_count = current_light_projections_count;
			lights_projections_buffer = std::make_unique<Buffer>(gfx, StructuredBufferDesc<XMMATRIX>(light_projections_count, false, true));
			lights_projections_buffer->CreateSRV();
		}
		std::vector<XMMATRIX> light_projections{};
		light_projections.reserve(light_projections_count);
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			if (light.casts_shadows)
			{
				if (light.ray_traced_shadows) continue;
				light.shadow_matrix_index = (uint32)light_projections.size();
				if (light.type == ELightType::Directional)
				{
					if (light.use_cascades)
					{
						std::array<XMMATRIX, shadows::SHADOW_CASCADE_COUNT> proj_matrices = shadows::RecalculateProjectionMatrices(*camera, cascades_split_lambda, split_distances);
						for (uint32 i = 0; i < shadows::SHADOW_CASCADE_COUNT; ++i)
						{
							auto const& [V, P] = shadows::LightViewProjection_Cascades(light, *camera, proj_matrices[i]);
							light_projections.push_back(V * P);
						}
					}
					else
					{
						auto const& [V, P] = shadows::LightViewProjection_Directional(light, *camera);
						light_projections.push_back(V * P);
					}
					
				}
				else if (light.type == ELightType::Point)
				{
					for (uint32 i = 0; i < 6; ++i)
					{
						auto const& [V, P] = shadows::LightViewProjection_Point(light, i);
						light_projections.push_back(V * P);
					}
				}
				else if (light.type == ELightType::Spot)
				{
					auto const& [V, P] = shadows::LightViewProjection_Spot(light);
					light_projections.push_back(V * P);
				}
			}
		}
		lights_projections_buffer->Update(light_projections.data(), light_projections.size() * sizeof(XMMATRIX));

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		OffsetType i = descriptor_allocator->Allocate();
		auto dst_descriptor = descriptor_allocator->GetHandle(i);
		device->CopyDescriptorsSimple(1, dst_descriptor, lights_projections_buffer->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		light_projections_srv = dst_descriptor;
	}
	void Renderer::UpdateLights()
	{
		auto light_view = reg.view<Light>();
		static size_t light_count = 0;
		if (light_count != light_view.size())
		{
			gfx->WaitForGPU();
			light_count = light_view.size();
			lights_buffer = std::make_unique<Buffer>(gfx, StructuredBufferDesc<LightHLSL>(light_count, false, true));
			lights_buffer->CreateSRV();
		}

		std::vector<LightHLSL> hlsl_lights{};
		hlsl_lights.reserve(light_view.size());
		for (auto e : light_view)
		{
			LightHLSL hlsl_light{};
			auto& light = light_view.get<Light>(e);
			hlsl_light.color = light.color * light.energy;
			hlsl_light.position = XMVector4Transform(light.position, camera->View());
			hlsl_light.direction = XMVector4Transform(light.direction, camera->View());
			hlsl_light.range = light.range;
			hlsl_light.type = static_cast<int>(light.type);
			hlsl_light.inner_cosine = light.inner_cosine;
			hlsl_light.outer_cosine = light.outer_cosine;
			hlsl_light.active = light.active;
			hlsl_light.shadow_matrix_index = light.shadow_matrix_index;
			hlsl_light.shadow_texture_index = light.shadow_texture_index;
			hlsl_light.use_cascades = light.use_cascades;
			hlsl_lights.push_back(hlsl_light);
		}
		lights_buffer->Update(hlsl_lights.data(), hlsl_lights.size() * sizeof(LightHLSL));

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

		static FrameCBuffer frame_cbuf_data{};
		{
			frame_cbuf_data.camera_near = camera->Near();
			frame_cbuf_data.camera_far = camera->Far();
			frame_cbuf_data.camera_position = camera->Position();
			frame_cbuf_data.camera_forward = camera->Forward();
			frame_cbuf_data.view = camera->View();
			frame_cbuf_data.projection = camera->Proj();
			frame_cbuf_data.view_projection = camera->ViewProj();
			frame_cbuf_data.inverse_view = XMMatrixInverse(nullptr, camera->View());
			frame_cbuf_data.inverse_projection = XMMatrixInverse(nullptr, camera->Proj());
			frame_cbuf_data.inverse_view_projection = XMMatrixInverse(nullptr, camera->ViewProj());
			frame_cbuf_data.reprojection = frame_cbuf_data.inverse_view_projection * frame_cbuf_data.prev_view_projection;
			frame_cbuf_data.screen_resolution_x = (float)width;
			frame_cbuf_data.screen_resolution_y = (float)height;
			frame_cbuf_data.delta_time = dt;
			frame_cbuf_data.total_time = total_time;
			frame_cbuf_data.frame_count = gfx->FrameIndex();
			frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
			frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;
			frame_cbuf_data.lights_idx = (int32)light_array_srv.GetHeapOffset();
			frame_cbuf_data.lights_projections_idx = (int32)light_projections_srv.GetHeapOffset();

			auto lights = reg.view<Light>();
			for (auto light : lights)
			{
				auto const& light_data = lights.get<Light>(light);
				if (light_data.type == ELightType::Directional && light_data.active)
				{
					frame_cbuf_data.sun_direction = -light_data.direction;
					frame_cbuf_data.sun_color = light_data.color;
					XMStoreFloat3(&sun_direction, -light_data.direction);
					break;
				}
			}
			frame_cbuf_data.wind_params = XMVectorSet(renderer_settings.wind_dir[0], renderer_settings.wind_dir[1], renderer_settings.wind_dir[2], renderer_settings.wind_speed);
			frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);
			frame_cbuf_data.prev_view_projection = camera->ViewProj();
		}

		static OldFrameCBuffer old_frame_cbuf_data{};
		//frame
		{
			old_frame_cbuf_data.camera_near = camera->Near();
			old_frame_cbuf_data.camera_far = camera->Far();
			old_frame_cbuf_data.camera_position = camera->Position();
			old_frame_cbuf_data.camera_forward = camera->Forward();
			old_frame_cbuf_data.view = camera->View();
			old_frame_cbuf_data.projection = camera->Proj();
			old_frame_cbuf_data.view_projection = camera->ViewProj();
			old_frame_cbuf_data.inverse_view = DirectX::XMMatrixInverse(nullptr, camera->View());
			old_frame_cbuf_data.inverse_projection = DirectX::XMMatrixInverse(nullptr, camera->Proj());
			old_frame_cbuf_data.inverse_view_projection = DirectX::XMMatrixInverse(nullptr, camera->ViewProj());
			old_frame_cbuf_data.screen_resolution_x = (float)width;
			old_frame_cbuf_data.screen_resolution_y = (float)height;
			old_frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
			old_frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;

			old_frame_cbuffer.Update(old_frame_cbuf_data, backbuffer_index);
			old_frame_cbuf_data.prev_view_projection = camera->ViewProj();
		}

		//compute 
		{
			static ComputeCBuffer compute_cbuf_data{};
			compute_cbuf_data.visualize_tiled = tiled_lighting_pass.IsVisualized();
			compute_cbuf_data.visualize_max_lights = tiled_lighting_pass.MaxLightsForVisualization();
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

