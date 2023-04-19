#include "Renderer.h"
#include "BlackboardData.h"
#include "Camera.h"
#include "Components.h"
#include "PSOCache.h"

#include "ShaderCache.h"
#include "SkyModel.h"
#include "TextureManager.h"
#include "entt/entity/registry.hpp"
#include "Editor/GUICommand.h"
#include "Editor/Editor.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxTexture.h"

#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/Random.h"
#include "Utilities/hwbp.h"
#include "Math/Halton.h"
#include "Logging/Logger.h"

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
			shadow_origin *= (SHADOW_MAP_SIZE / 2.0f);

			XMVECTOR rounded_origin = XMVectorRound(shadow_origin);
			XMVECTOR rounded_offset = rounded_origin - shadow_origin;
			rounded_offset *= (2.0f / SHADOW_MAP_SIZE);
			rounded_offset *= XMVectorSet(1.0, 1.0, 0.0, 0.0);

			P.r[3] += rounded_offset;
			return { V,P };
		}
		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Spot(Light const& light)
		{
			ADRIA_ASSERT(light.type == LightType::Spot);

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

	Renderer::Renderer(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height) : reg(reg), gfx(gfx), resource_pool(gfx),
		accel_structure(gfx), camera(nullptr), width(width), height(height),
		backbuffer_count(gfx->BackbufferCount()), backbuffer_index(gfx->BackbufferIndex()), final_texture(nullptr),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count),
		gbuffer_pass(reg, width, height), ambient_pass(width, height), tonemap_pass(width, height),
		sky_pass(reg, width, height), deferred_lighting_pass(width, height), volumetric_lighting_pass(width, height),
		tiled_deferred_lighting_pass(reg, width, height) , copy_to_texture_pass(width, height), add_textures_pass(width, height),
		postprocessor(reg, width, height), fxaa_pass(width, height), picking_pass(gfx, width, height),
		clustered_deferred_lighting_pass(reg, gfx, width, height), ssao_pass(width, height), hbao_pass(width, height),
		decals_pass(reg, width, height), ocean_renderer(reg, width, height), aabb_pass(reg, width, height),
		ray_traced_shadows_pass(gfx, width, height), rtao_pass(gfx, width, height), rtr_pass(gfx, width, height),
		path_tracer(gfx, width, height), ray_tracing_supported(gfx->GetCapabilities().SupportsRayTracing())
	{
		g_GpuProfiler.Init(gfx);
		CreateSizeDependentResources();
	}

	Renderer::~Renderer()
	{
		g_GpuProfiler.Destroy();
		gfx->WaitForGPU();
		reg.clear();
		gfxcommon::Destroy();
	}
	void Renderer::NewFrame(Camera const* _camera)
	{
		ADRIA_ASSERT(_camera);
		camera = _camera;
		backbuffer_index = gfx->BackbufferIndex();
		g_GpuProfiler.NewFrame();
	}
	void Renderer::Update(float dt)
	{
		SetupShadows();
		UpdateSceneBuffers();
		UpdateEnvironmentMap();
		UpdateFrameConstantBuffer(dt);
		CameraFrustumCulling();
		MiscGUI();
	}
	void Renderer::Render(RendererSettings const& _settings)
	{
		renderer_settings = _settings;
		g_TextureManager.Tick();

		RenderGraph render_graph(resource_pool);
		RGBlackboard& rg_blackboard = render_graph.GetBlackboard();
		FrameBlackboardData global_data{};
		{
			global_data.camera_position = camera->Position();
			global_data.camera_view = camera->View();
			global_data.camera_proj = camera->Proj();
			global_data.camera_viewproj = camera->ViewProj();
			global_data.camera_fov = camera->Fov();
			global_data.frame_cbuffer_address = frame_cbuffer.BufferLocation(backbuffer_index);
		}
		rg_blackboard.Add<FrameBlackboardData>(std::move(global_data));

		render_graph.ImportTexture(RG_RES_NAME(Backbuffer), gfx->GetBackbuffer());

		if (renderer_settings.render_path == RenderPathType::PathTracing) Render_PathTracing(render_graph);
		else Render_Deferred(render_graph);

		render_graph.Build();
		render_graph.Execute();
	}

	void Renderer::SetViewportData(ViewportData const& vp)
	{
		viewport_data = vp;
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
			volumetric_lighting_pass.OnResize(w, h);
			tiled_deferred_lighting_pass.OnResize(w, h);
			clustered_deferred_lighting_pass.OnResize(w, h);
			copy_to_texture_pass.OnResize(w, h);
			tonemap_pass.OnResize(w, h);
			fxaa_pass.OnResize(w, h);
			postprocessor.OnResize(gfx, w, h);
			add_textures_pass.OnResize(w, h);
			picking_pass.OnResize(w, h);
			decals_pass.OnResize(w, h);
			ocean_renderer.OnResize(w, h);
			ray_traced_shadows_pass.OnResize(w, h);
			rtr_pass.OnResize(w, h);
			path_tracer.OnResize(w, h);
			rtao_pass.OnResize(w, h);
			aabb_pass.OnResize(w, h);

			light_mask_textures.clear();
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
		tonemap_pass.OnSceneInitialized(gfx);
		CreateAS();

		gfxcommon::Initialize(gfx);
		g_TextureManager.OnSceneInitialized();
	}
	void Renderer::OnRightMouseClicked(int32 x, int32 y)
	{
		update_picking_data = true;
	}

	void Renderer::CreateSizeDependentResources()
	{
		GfxTextureDesc ldr_desc{};
		ldr_desc.width = width;
		ldr_desc.height = height;
		ldr_desc.format = GfxFormat::R10G10B10A2_UNORM;
		ldr_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource | GfxBindFlag::RenderTarget;
		ldr_desc.initial_state = GfxResourceState::UnorderedAccess;

		final_texture = std::make_unique<GfxTexture>(gfx, ldr_desc);
		final_texture_srv = gfx->CreateTextureSRV(final_texture.get());
	}
	void Renderer::CreateAS()
	{
		if (!ray_tracing_supported) return;
		if (reg.view<RayTracing>().size() == 0) return;

		auto ray_tracing_view = reg.view<Mesh, RayTracing>();
		for (auto entity : ray_tracing_view)
		{
			auto const& mesh = ray_tracing_view.get<Mesh>(entity);
			accel_structure.AddInstance(mesh);
		}
		accel_structure.Build();
		tlas_srv = gfx->CreateBufferSRV(&*accel_structure.GetTLAS());
	}

	void Renderer::UpdateEnvironmentMap()
	{
		GfxDescriptor env_map = gfxcommon::GetCommonView(GfxCommonViewType::NullTextureCube_SRV);
		if (sky_pass.GetSkyType() == SkyType::Skybox)
		{
			auto skybox_entities = reg.view<Skybox>();
			for (auto e : skybox_entities)
			{
				Skybox skybox = skybox_entities.get<Skybox>(e);
				if (skybox.active)
				{
					env_map = g_TextureManager.GetSRV(skybox.cubemap_texture);
					break;
				}
			}
		}
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		GfxDescriptor descriptor = descriptor_allocator->Allocate();
		gfx->CopyDescriptors(1, descriptor, env_map);
	}
	void Renderer::SetupShadows()
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		auto AddShadowMask = [&](Light& light, size_t light_id)
		{
			if (light_mask_textures[light_id] == nullptr)
			{
				GfxTextureDesc mask_desc{};
				mask_desc.width = width;
				mask_desc.height = height;
				mask_desc.format = GfxFormat::R8_UNORM;
				mask_desc.initial_state = GfxResourceState::UnorderedAccess;
				mask_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::UnorderedAccess;

				light_mask_textures[light_id] = std::make_unique<GfxTexture>(gfx, mask_desc);

				light_mask_texture_srvs[light_id] = gfx->CreateTextureSRV(light_mask_textures[light_id].get());
				light_mask_texture_uavs[light_id] = gfx->CreateTextureUAV(light_mask_textures[light_id].get());
			}

			GfxDescriptor srv = light_mask_texture_srvs[light_id];
			GfxDescriptor dst_descriptor = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, dst_descriptor, srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			light.shadow_mask_index = (int32)dst_descriptor.GetIndex();
		};
		auto AddShadowMap = [&](size_t light_id, uint32 shadow_map_size)
		{
			GfxTextureDesc depth_desc{};
			depth_desc.width = shadow_map_size;
			depth_desc.height = shadow_map_size;
			depth_desc.format = GfxFormat::R32_TYPELESS;
			depth_desc.clear_value = GfxClearValue(1.0f, 0);
			depth_desc.bind_flags = GfxBindFlag::DepthStencil | GfxBindFlag::ShaderResource;
			depth_desc.initial_state = GfxResourceState::DepthWrite;

			light_shadow_maps[light_id].emplace_back(std::make_unique<GfxTexture>(gfx, depth_desc));
			light_shadow_map_srvs[light_id].push_back(gfx->CreateTextureSRV(light_shadow_maps[light_id].back().get()));
			light_shadow_map_dsvs[light_id].push_back(gfx->CreateTextureDSV(light_shadow_maps[light_id].back().get()));
		};
		auto AddShadowMaps = [&](Light& light, size_t light_id)
		{
			switch(light.type)
			{
			case LightType::Directional:
			{
				if (light.use_cascades && light_shadow_maps[light_id].size() != shadows::SHADOW_CASCADE_COUNT)
				{
					light_shadow_maps[light_id].clear();
					for(uint32 i = 0; i < shadows::SHADOW_CASCADE_COUNT; ++i) AddShadowMap(light_id, shadows::SHADOW_CASCADE_MAP_SIZE);
				}
				else if (!light.use_cascades && light_shadow_maps[light_id].size() != 1)
				{
					light_shadow_maps[light_id].clear();
					AddShadowMap(light_id, shadows::SHADOW_MAP_SIZE);
				}
			}
			break;
			case LightType::Point:
			{
				if (light_shadow_maps[light_id].size() != 6)
				{
					light_shadow_maps[light_id].clear();
					for (uint32 i = 0; i < 6; ++i) AddShadowMap(light_id, shadows::SHADOW_CUBE_SIZE);
				}
			}
			break;
			case LightType::Spot:
			{
				if (light_shadow_maps[light_id].size() != 1)
				{
					light_shadow_maps[light_id].clear();
					AddShadowMap(light_id, shadows::SHADOW_MAP_SIZE);
				}
			}
			break;
			}

			for (size_t j = 0; j < light_shadow_maps[light_id].size(); ++j)
			{
				GfxDescriptor srv = light_shadow_map_srvs[light_id][j];
				GfxDescriptor dst_descriptor = descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, dst_descriptor, srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				if(j == 0) light.shadow_texture_index = (int32)dst_descriptor.GetIndex();
			}
		};

		static size_t light_matrices_count = 0;
		size_t current_light_matrices_count = 0;

		auto light_view = reg.view<Light>();
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			if (light.casts_shadows)
			{
				if (light.type == LightType::Directional && light.use_cascades) current_light_matrices_count += shadows::SHADOW_CASCADE_COUNT;
				else if (light.type == LightType::Point) current_light_matrices_count += 6;
				else current_light_matrices_count++;
			}
		}

		if (current_light_matrices_count != light_matrices_count)
		{
			gfx->WaitForGPU();
			light_matrices_count = current_light_matrices_count;
			if (light_matrices_count != 0)
			{
				light_matrices_buffer = std::make_unique<GfxBuffer>(gfx, StructuredBufferDesc<XMMATRIX>(light_matrices_count * backbuffer_count, false, true));
				GfxBufferSubresourceDesc srv_desc{};
				srv_desc.size = light_matrices_count * sizeof(XMMATRIX);
				for (uint32 i = 0; i < backbuffer_count; ++i)
				{
					srv_desc.offset = i * light_matrices_count * sizeof(XMMATRIX);
					light_matrices_buffer_srvs[i] = gfx->CreateBufferSRV(light_matrices_buffer.get(), &srv_desc);
				}
			}
		}

		std::vector<XMMATRIX> light_matrices;
		light_matrices.reserve(light_matrices_count);
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			light.shadow_mask_index = -1;
			light.shadow_texture_index = -1;
			if (light.casts_shadows)
			{
				if (light.ray_traced_shadows) continue;
				light.shadow_matrix_index = (uint32)light_matrices.size();
				if (light.type == LightType::Directional)
				{
					if (light.use_cascades)
					{
						std::array<XMMATRIX, shadows::SHADOW_CASCADE_COUNT> proj_matrices = shadows::RecalculateProjectionMatrices(*camera, cascades_split_lambda, split_distances);
						for (uint32 i = 0; i < shadows::SHADOW_CASCADE_COUNT; ++i)
						{
							AddShadowMaps(light, entt::to_integral(e));
							auto const& [V, P] = shadows::LightViewProjection_Cascades(light, *camera, proj_matrices[i]);
							light_matrices.push_back(XMMatrixTranspose(V * P));
						}
					}
					else
					{
						AddShadowMaps(light, entt::to_integral(e));
						auto const& [V, P] = shadows::LightViewProjection_Directional(light, *camera);
						light_matrices.push_back(XMMatrixTranspose(V * P));
					}

				}
				else if (light.type == LightType::Point)
				{
					for (uint32 i = 0; i < 6; ++i)
					{
						AddShadowMaps(light, entt::to_integral(e));
						auto const& [V, P] = shadows::LightViewProjection_Point(light, i);
						light_matrices.push_back(XMMatrixTranspose(V * P));
					}
				}
				else if (light.type == LightType::Spot)
				{
					AddShadowMaps(light, entt::to_integral(e));
					auto const& [V, P] = shadows::LightViewProjection_Spot(light);
					light_matrices.push_back(XMMatrixTranspose(V * P));
				}
			}
			else if(light.ray_traced_shadows)
			{
				AddShadowMask(light, entt::to_integral(e));
			}
		}
		if (light_matrices_buffer)
		{
			light_matrices_buffer->Update(light_matrices.data(), light_matrices_count * sizeof(XMMATRIX), light_matrices_count * sizeof(XMMATRIX) * backbuffer_index);
			GfxDescriptor dst_descriptor = descriptor_allocator->Allocate();
			gfx->CopyDescriptors(1, dst_descriptor, light_matrices_buffer_srvs[backbuffer_index]);
			light_matrices_srv_gpu = dst_descriptor;
		}
	}

	void Renderer::UpdateSceneBuffers()
	{
		volumetric_lights = 0;
		for (auto e : reg.view<Batch>())
			reg.destroy(e);
		reg.clear<Batch>();

		std::vector<LightHLSL> hlsl_lights{};
		uint32 light_index = 0;
		XMMATRIX light_transform = renderer_settings.render_path == RenderPathType::PathTracing ? XMMatrixIdentity() : camera->View();
		for (auto light_entity : reg.view<Light>())
		{
			Light& light = reg.get<Light>(light_entity);
			light.light_index = light_index;
			++light_index;

			LightHLSL& hlsl_light = hlsl_lights.emplace_back();
			hlsl_light.color = light.color * light.energy;
			hlsl_light.position = XMVector4Transform(light.position, light_transform);
			hlsl_light.direction = XMVector4Transform(light.direction, light_transform);
			hlsl_light.range = light.range;
			hlsl_light.type = static_cast<int32>(light.type);
			hlsl_light.inner_cosine = light.inner_cosine;
			hlsl_light.outer_cosine = light.outer_cosine;
			hlsl_light.volumetric = light.volumetric;
			hlsl_light.volumetric_strength = light.volumetric_strength;
			hlsl_light.active = light.active;
			hlsl_light.shadow_matrix_index = light.casts_shadows ? light.shadow_matrix_index : -1;
			hlsl_light.shadow_texture_index = light.casts_shadows ? light.shadow_texture_index : -1;
			hlsl_light.shadow_mask_index = light.ray_traced_shadows ? light.shadow_mask_index : -1;
			hlsl_light.use_cascades = light.use_cascades;
			if (light.volumetric) ++volumetric_lights;
		}

		std::vector<MeshHLSL> meshes;
		std::vector<InstanceHLSL> instances;
		std::vector<MaterialHLSL> materials;
		uint32 instanceID = 0;

		for (auto mesh_entity : reg.view<Mesh>())
		{
			Mesh& mesh = reg.get<Mesh>(mesh_entity);

			GfxBuffer* mesh_buffer = g_GeometryBufferCache.GetGeometryBuffer(mesh.geometry_buffer_handle);
			GfxDescriptor mesh_buffer_srv = g_GeometryBufferCache.GetGeometryBufferSRV(mesh.geometry_buffer_handle);
			GfxDescriptor mesh_buffer_online_srv = gfx->GetDescriptorAllocator()->Allocate();
			gfx->CopyDescriptors(1, mesh_buffer_online_srv, mesh_buffer_srv);

			for (auto const& instance : mesh.instances)
			{
				SubMeshGPU& submesh = mesh.submeshes[instance.submesh_index];
				Material& material = mesh.materials[submesh.material_index];

				submesh.buffer_address = mesh_buffer->GetGpuAddress();

				entt::entity batch_entity = reg.create();
				Batch& batch = reg.emplace<Batch>(batch_entity);
				batch.instance_id = instanceID;
				batch.alpha_mode = material.alpha_mode;
				batch.submesh = &submesh;
				batch.world_transform = instance.world_transform;
				submesh.bounding_box.Transform(batch.bounding_box, batch.world_transform);

				InstanceHLSL& instance_hlsl = instances.emplace_back();
				instance_hlsl.instance_id = instanceID;
				instance_hlsl.material_idx = static_cast<uint32>(materials.size() + submesh.material_index);
				instance_hlsl.mesh_index = static_cast<uint32>(meshes.size() + instance.submesh_index);
				instance_hlsl.world_matrix = instance.world_transform;
				instance_hlsl.inverse_world_matrix = XMMatrixInverse(nullptr, instance.world_transform);
				instance_hlsl.bb_origin = submesh.bounding_box.Center;
				instance_hlsl.bb_extents = submesh.bounding_box.Extents;

				++instanceID;
			}
			for (auto const& submesh : mesh.submeshes)
			{
				MeshHLSL& mesh_hlsl = meshes.emplace_back();
				mesh_hlsl.buffer_idx = mesh_buffer_online_srv.GetIndex();
				mesh_hlsl.indices_offset = submesh.indices_offset;
				mesh_hlsl.positions_offset = submesh.positions_offset;
				mesh_hlsl.normals_offset = submesh.normals_offset;
				mesh_hlsl.tangents_offset = submesh.tangents_offset;
				mesh_hlsl.bitangents_offset = submesh.bitangents_offset;
				mesh_hlsl.uvs_offset = submesh.uvs_offset;

				mesh_hlsl.meshlet_offset = submesh.meshlet_offset;
				mesh_hlsl.meshlet_vertices_offset = submesh.meshlet_vertices_offset;
				mesh_hlsl.meshlet_triangles_offset = submesh.meshlet_triangles_offset;
				mesh_hlsl.meshlet_count = submesh.meshlet_count;
			}

			for (auto const& material : mesh.materials)
			{
				MaterialHLSL& material_hlsl = materials.emplace_back();
				material_hlsl.diffuse_idx = (uint32)material.albedo_texture;
				material_hlsl.normal_idx = (uint32)material.normal_texture;
				material_hlsl.roughness_metallic_idx = (uint32)material.metallic_roughness_texture;
				material_hlsl.emissive_idx = (uint32)material.emissive_texture;
				material_hlsl.base_color_factor = XMFLOAT3(material.base_color);
				material_hlsl.emissive_factor = material.emissive_factor;
				material_hlsl.metallic_factor = material.metallic_factor;
				material_hlsl.roughness_factor = material.roughness_factor;
				material_hlsl.alpha_cutoff = material.alpha_cutoff;
			}
		}

		auto CopyBuffer = [&]<typename T>(std::vector<T> const& data, SceneBuffer& scene_buffer)
		{
			if (data.empty()) return;
			if (!scene_buffer.buffer || scene_buffer.buffer->GetCount() < data.size())
			{
				scene_buffer.buffer = std::make_unique<GfxBuffer>(gfx, StructuredBufferDesc<T>(data.size(), false, true));
				scene_buffer.buffer_srv = gfx->CreateBufferSRV(scene_buffer.buffer.get());
			}
			scene_buffer.buffer->Update(data.data(), data.size() * sizeof(T));
			scene_buffer.buffer_srv_gpu = gfx->GetDescriptorAllocator()->Allocate();
			gfx->CopyDescriptors(1, scene_buffer.buffer_srv_gpu, scene_buffer.buffer_srv);
		};
		CopyBuffer(hlsl_lights, scene_buffers[SceneBuffer_Light]);
		CopyBuffer(meshes, scene_buffers[SceneBuffer_Mesh]);
		CopyBuffer(instances, scene_buffers[SceneBuffer_Instance]);
		CopyBuffer(materials, scene_buffers[SceneBuffer_Material]);

		size_t count = reg.size();
	}

	void Renderer::UpdateFrameConstantBuffer(float dt)
	{
		auto AreMatricesEqual = [](XMMATRIX m1, XMMATRIX m2) -> bool
		{
			XMFLOAT4X4 _m1, _m2;
			XMStoreFloat4x4(&_m1, m1);
			XMStoreFloat4x4(&_m2, m2);

			return !memcmp(_m1.m[0], _m2.m[0], 4 * sizeof(float)) &&
				!memcmp(_m1.m[1], _m2.m[1], 4 * sizeof(float)) &&
				!memcmp(_m1.m[2], _m2.m[2], 4 * sizeof(float)) &&
				!memcmp(_m1.m[3], _m2.m[3], 4 * sizeof(float));
		};

		static float total_time = 0.0f;
		total_time += dt;

		float jitter_x = 0.0f, jitter_y = 0.0f;
		if (HasAllFlags(renderer_settings.postprocess.anti_aliasing, AntiAliasing_TAA))
		{
			constexpr HaltonSequence<16, 2> x;
			constexpr HaltonSequence<16, 3> y;
			jitter_x = x[gfx->FrameIndex() % 16];
			jitter_y = y[gfx->FrameIndex() % 16];
			jitter_x = ((jitter_x - 0.5f) / width) * 2;
			jitter_y = ((jitter_y - 0.5f) / height) * 2;
		}

		static FrameCBuffer frame_cbuf_data{};
		if (!AreMatricesEqual(camera->ViewProj(), frame_cbuf_data.prev_view_projection)) path_tracer.Reset();
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
		frame_cbuf_data.camera_jitter_x = jitter_x;
		frame_cbuf_data.camera_jitter_y = jitter_y;
		frame_cbuf_data.screen_resolution_x = (float)width;
		frame_cbuf_data.screen_resolution_y = (float)height;
		frame_cbuf_data.delta_time = dt;
		frame_cbuf_data.total_time = total_time;
		frame_cbuf_data.frame_count = gfx->FrameIndex();
		frame_cbuf_data.mouse_normalized_coords_x = (viewport_data.mouse_position_x - viewport_data.scene_viewport_pos_x) / viewport_data.scene_viewport_size_x;
		frame_cbuf_data.mouse_normalized_coords_y = (viewport_data.mouse_position_y - viewport_data.scene_viewport_pos_y) / viewport_data.scene_viewport_size_y;

		frame_cbuf_data.meshes_idx = (int32)scene_buffers[SceneBuffer_Mesh].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.materials_idx = (int32)scene_buffers[SceneBuffer_Material].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.instances_idx = (int32)scene_buffers[SceneBuffer_Instance].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.lights_idx = (int32)scene_buffers[SceneBuffer_Light].buffer_srv_gpu.GetIndex();
		frame_cbuf_data.lights_matrices_idx = (int32)light_matrices_srv_gpu.GetIndex();
		frame_cbuf_data.env_map_idx = (int32)envmap_srv_gpu.GetIndex();
		frame_cbuf_data.cascade_splits = XMVectorSet(split_distances[0], split_distances[1], split_distances[2], split_distances[3]);
		auto lights = reg.view<Light>();
		for (auto light : lights)
		{
			auto const& light_data = lights.get<Light>(light);
			if (light_data.type == LightType::Directional && light_data.active)
			{
				frame_cbuf_data.sun_direction = -light_data.direction;
				frame_cbuf_data.sun_color = light_data.color;
				XMStoreFloat3(&sun_direction, -light_data.direction);
				break;
			}
		}

		if (ray_tracing_supported && reg.view<RayTracing>().size())
		{
			auto descriptor_allocator = gfx->GetDescriptorAllocator();
			GfxDescriptor accel_struct_descriptor = descriptor_allocator->Allocate();
			gfx->CopyDescriptors(1, accel_struct_descriptor, tlas_srv);
			frame_cbuf_data.accel_struct_idx = (int32)accel_struct_descriptor.GetIndex();
		}

		frame_cbuf_data.wind_params = XMVectorSet(wind_dir[0], wind_dir[1], wind_dir[2], wind_speed);
		frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);
		frame_cbuf_data.prev_view_projection = camera->ViewProj();
	}
	void Renderer::CameraFrustumCulling()
	{
		BoundingFrustum camera_frustum = camera->Frustum();
		auto batch_view = reg.view<Batch>();
		for (auto e : batch_view)
		{
			Batch& batch = batch_view.get<Batch>(e);
			auto& aabb = batch.bounding_box;
			batch.camera_visibility = camera_frustum.Intersects(aabb);
		}
	}

	void Renderer::Render_Deferred(RenderGraph& render_graph)
	{
		if (update_picking_data)
		{
			picking_data = picking_pass.GetPickingData();
			update_picking_data = false;
		}

		gbuffer_pass.AddPass(render_graph);
		decals_pass.AddPass(render_graph);
		switch (renderer_settings.postprocess.ambient_occlusion)
		{
		case AmbientOcclusion::SSAO:
		{
			ssao_pass.AddPass(render_graph);
			break;
		}
		case AmbientOcclusion::HBAO:
		{
			hbao_pass.AddPass(render_graph);
			break;
		}
		case AmbientOcclusion::RTAO:
		{
			rtao_pass.AddPass(render_graph);
			break;
		}
		case AmbientOcclusion::None:
		default:
			break;
		}
		ambient_pass.AddPass(render_graph);
		AddShadowMapPasses(render_graph);
		AddRayTracingShadowPasses(render_graph);
		switch (renderer_settings.render_path)
		{
		case RenderPathType::RegularDeferred:
			deferred_lighting_pass.AddPass(render_graph);
			break;
		case RenderPathType::TiledDeferred:
			tiled_deferred_lighting_pass.AddPass(render_graph);
			break;
		case RenderPathType::ClusteredDeferred:
			clustered_deferred_lighting_pass.AddPass(render_graph, true);
			break;
		}
		if (volumetric_lights > 0) volumetric_lighting_pass.AddPass(render_graph);

		aabb_pass.AddPass(render_graph);
		ocean_renderer.AddPasses(render_graph);
		sky_pass.AddPass(render_graph, sun_direction);
		picking_pass.AddPass(render_graph);
		if (renderer_settings.postprocess.reflections == Reflections::RTR) rtr_pass.AddPass(render_graph);
		postprocessor.AddPasses(render_graph, renderer_settings.postprocess);

		render_graph.ImportTexture(RG_RES_NAME(FinalTexture), final_texture.get());
		ResolveToFinalTexture(render_graph);
		if (!Editor::Get().IsActive()) CopyToBackbuffer(render_graph);
		else Editor::Get().AddRenderPass(render_graph);
	}
	void Renderer::Render_PathTracing(RenderGraph& render_graph)
	{
		path_tracer.AddPass(render_graph);
		render_graph.ImportTexture(RG_RES_NAME(FinalTexture), final_texture.get());
		tonemap_pass.AddPass(render_graph, RG_RES_NAME(PT_Output));
		if (!Editor::Get().IsActive()) CopyToBackbuffer(render_graph);
		else Editor::Get().AddRenderPass(render_graph);
	}

	void Renderer::MiscGUI()
	{
		AddGUI([&]() {

			if (ImGui::TreeNode("Misc"))
			{
				ImGui::SliderFloat3("Wind direction", wind_dir, 0.0f, 1.0f);
				ImGui::SliderFloat("Wind speed", &wind_speed, 0.0f, 10.0f);
				ImGui::TreePop();
			}
			});
	}

	void Renderer::ShadowMapPass_Common(GfxDevice* gfx, GfxCommandList* cmd_list, bool transparent, size_t light_index, size_t shadow_map_index)
	{
		struct ShadowConstants
		{
			uint32  light_index;
			uint32  matrix_index;
		} constants =
		{
			.light_index = (uint32)light_index,
			.matrix_index = (uint32)shadow_map_index
		};

		cmd_list->SetRootConstants(1, constants);
		for (auto batch_entity : reg.view<Batch>())
		{
			Batch& batch = reg.get<Batch>(batch_entity);
			GfxPipelineStateID pso_id = GfxPipelineStateID::Shadow;
			cmd_list->SetPipelineState(PSOCache::Get(pso_id));

			struct ModelConstants
			{
				uint32 instance_id;
			} model_constants{ .instance_id = batch.instance_id };
			cmd_list->SetRootCBV(2, model_constants);

			GfxIndexBufferView ibv(batch.submesh->buffer_address + batch.submesh->indices_offset, batch.submesh->indices_count);
			cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
			cmd_list->SetIndexBuffer(&ibv);
			cmd_list->DrawIndexed(batch.submesh->indices_count);
		}
	}
	void Renderer::AddShadowMapPasses(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		auto light_view = reg.view<Light>();
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			if (!light.casts_shadows) continue;
			int32 light_index = light.light_index;
			size_t light_id = entt::to_integral(e);

			if (light.type == LightType::Directional)
			{
				if (light.use_cascades)
				{
					for (uint32 i = 0; i < shadows::SHADOW_CASCADE_COUNT; ++i)
					{
						rg.ImportTexture(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index + i), light_shadow_maps[light_id][i].get());
						std::string name = "Cascade Shadow Pass" + std::to_string(i);
						rg.AddPass<void>(name.c_str(),
						[=](RenderGraphBuilder& builder)
						{
							builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index + i), RGLoadStoreAccessOp::Clear_Preserve);
							builder.SetViewport(shadows::SHADOW_CASCADE_MAP_SIZE, shadows::SHADOW_CASCADE_MAP_SIZE);
						},
						[=](RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
						{
							cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
							ShadowMapPass_Common(gfx, cmd_list, false, light_index, i);
						}, RGPassType::Graphics);
					}
				}
				else
				{
					rg.ImportTexture(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index), light_shadow_maps[light_id][0].get());
					std::string name = "Directional Shadow Pass";
					rg.AddPass<void>(name.c_str(),
					[=](RenderGraphBuilder& builder)
					{
						builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index), RGLoadStoreAccessOp::Clear_Preserve);
						builder.SetViewport(shadows::SHADOW_MAP_SIZE, shadows::SHADOW_MAP_SIZE);
					},
					[=](RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
					{
						cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
						ShadowMapPass_Common(gfx, cmd_list, false, light_index);
					}, RGPassType::Graphics);
				}
			}
			else if (light.type == LightType::Point)
			{
				for (uint32 i = 0; i < 6; ++i)
				{
					rg.ImportTexture(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index + i), light_shadow_maps[light_id][i].get());
					std::string name = "Point Shadow Pass" + std::to_string(i);
					rg.AddPass<void>(name.c_str(),
						[=](RenderGraphBuilder& builder)
						{
							builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index + i), RGLoadStoreAccessOp::Clear_Preserve);
							builder.SetViewport(shadows::SHADOW_CUBE_SIZE, shadows::SHADOW_CUBE_SIZE);
						},
						[=](RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
						{
							cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
							ShadowMapPass_Common(gfx, cmd_list, false, light_index, i);
						}, RGPassType::Graphics);
				}
			}
			else if (light.type == LightType::Spot)
			{
				rg.ImportTexture(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index), light_shadow_maps[light_id][0].get());
				std::string name = "Spot Shadow Pass";
				rg.AddPass<void>(name.c_str(),
					[=](RenderGraphBuilder& builder)
					{
						builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light.shadow_matrix_index), RGLoadStoreAccessOp::Clear_Preserve);
						builder.SetViewport(shadows::SHADOW_MAP_SIZE, shadows::SHADOW_MAP_SIZE);
					},
					[=](RenderGraphContext& context, GfxDevice* gfx, GfxCommandList* cmd_list)
					{
						cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
						ShadowMapPass_Common(gfx, cmd_list, false, light_index);
					}, RGPassType::Graphics);
			}
		}
	}

	void Renderer::AddRayTracingShadowPasses(RenderGraph& rg)
	{
		auto light_view = reg.view<Light>();
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			if (!light.ray_traced_shadows) continue;
			int32 light_index = light.light_index;
			size_t light_id = entt::to_integral(e);

			rg.ImportTexture(RG_RES_NAME_IDX(LightMask, light_id), light_mask_textures[light_id].get());
			ray_traced_shadows_pass.AddPass(rg, light_index, RG_RES_NAME_IDX(LightMask, light_id));
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
				data.dst = builder.WriteCopyDstTexture(RG_RES_NAME(Backbuffer));
				data.src = builder.ReadCopySrcTexture(RG_RES_NAME(FinalTexture));
			},
			[=](CopyToBackbufferPassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, GfxCommandList* cmd_list)
			{
				GfxTexture const& src_texture = ctx.GetCopySrcTexture(data.src);
				GfxTexture& dst_texture = ctx.GetCopyDstTexture(data.dst);
				cmd_list->CopyTexture(dst_texture, src_texture);
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);
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

