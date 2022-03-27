
#include "../Tasks/TaskSystem.h"
#include "../Utilities/Random.h"
#include "Renderer.h"
#include <algorithm>
#include <iterator>
#include "Renderer.h"
#include "Camera.h"
#include "Components.h"
#include "SkyModel.h"
#include "../Core/Window.h"
#include "../Utilities/Timer.h"
#include "../Math/Constants.h"
#include "../Logging/Logger.h"
#include "../Editor/GUI.h"
#include "../Graphics/DWParam.h"
#include "pix3.h"

using namespace DirectX;
using namespace Microsoft::WRL;



namespace adria
{
	namespace shadow_helpers
	{
		constexpr uint32 SHADOW_MAP_SIZE = 2048;
		constexpr uint32 SHADOW_CUBE_SIZE = 512;
		constexpr uint32 SHADOW_CASCADE_MAP_SIZE = 1024;
		constexpr uint32 CASCADE_COUNT = 3;

		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Directional(Light const& light, BoundingSphere const&
			scene_bounding_sphere, BoundingBox& cull_box)
		{
			static const XMVECTOR sphere_center = DirectX::XMLoadFloat3(&scene_bounding_sphere.Center);

			XMVECTOR light_dir = XMVector3Normalize(light.direction);
			XMVECTOR light_pos = -1.0f * scene_bounding_sphere.Radius * light_dir + sphere_center;
			XMVECTOR target_pos = XMLoadFloat3(&scene_bounding_sphere.Center);
			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			XMMATRIX V = XMMatrixLookAtLH(light_pos, target_pos, up);
			XMFLOAT3 sphere_centerLS;
			XMStoreFloat3(&sphere_centerLS, XMVector3TransformCoord(target_pos, V));
			float32 l = sphere_centerLS.x - scene_bounding_sphere.Radius;
			float32 b = sphere_centerLS.y - scene_bounding_sphere.Radius;
			float32 n = sphere_centerLS.z - scene_bounding_sphere.Radius;
			float32 r = sphere_centerLS.x + scene_bounding_sphere.Radius;
			float32 t = sphere_centerLS.y + scene_bounding_sphere.Radius;
			float32 f = sphere_centerLS.z + scene_bounding_sphere.Radius;

			XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

			BoundingBox::CreateFromPoints(cull_box, XMVectorSet(l, b, n, 1.0f), XMVectorSet(r, t, f, 1.0f));
			cull_box.Transform(cull_box, XMMatrixInverse(nullptr, V));

			return { V,P };
		}

		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Directional(Light const& light, Camera const& camera,
			BoundingBox& cull_box)
		{
			BoundingFrustum frustum = camera.Frustum();
			std::array<XMFLOAT3, frustum.CORNER_COUNT> corners;
			frustum.GetCorners(corners.data());

			BoundingSphere frustumSphere;
			BoundingSphere::CreateFromFrustum(frustumSphere, frustum);

			XMVECTOR frustumCenter = XMVectorSet(0, 0, 0, 0);
			for (uint32 i = 0; i < corners.size(); ++i)
			{
				frustumCenter = frustumCenter + XMLoadFloat3(&corners[i]);
			}
			frustumCenter /= static_cast<float32>(corners.size());

			float32 radius = 0.0f;

			for (uint32 i = 0; i < corners.size(); ++i)
			{
				float32 dist = DirectX::XMVectorGetX(XMVector3Length(DirectX::XMLoadFloat3(&corners[i]) - frustumCenter));
				radius = (std::max)(radius, dist);
			}
			radius = std::ceil(radius * 8.0f) / 8.0f;

			XMVECTOR const maxExtents = XMVectorSet(radius, radius, radius, 0);
			XMVECTOR const minExtents = -maxExtents;
			XMVECTOR const cascadeExtents = maxExtents - minExtents;

			XMVECTOR lightDir = XMVector3Normalize(light.direction);
			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMMATRIX V = XMMatrixLookAtLH(frustumCenter, frustumCenter + 1.0f * lightDir * radius, up);

			XMFLOAT3 minE, maxE, cascadeE;

			XMStoreFloat3(&minE, minExtents);
			XMStoreFloat3(&maxE, maxExtents);
			XMStoreFloat3(&cascadeE, cascadeExtents);

			float32 l = minE.x;
			float32 b = minE.y;
			float32 n = minE.z; // -farFactor * radius;
			float32 r = maxE.x;
			float32 t = maxE.y;
			float32 f = maxE.z * 1.5f;

			XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

			XMMATRIX VP = V * P;

			XMFLOAT3 so(0, 0, 0);

			XMVECTOR shadowOrigin = XMLoadFloat3(&so);

			shadowOrigin = XMVector3Transform(shadowOrigin, VP); //sOrigin * P;
			shadowOrigin *= (SHADOW_MAP_SIZE / 2.0f);

			XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);
			XMVECTOR roundOffset = roundedOrigin - shadowOrigin;

			roundOffset *= (2.0f / SHADOW_MAP_SIZE);
			roundOffset *= XMVectorSet(1.0, 1.0, 0.0, 0.0);

			P.r[3] += roundOffset;
			BoundingBox::CreateFromPoints(cull_box, XMVectorSet(l, b, n, 1.0f), XMVectorSet(r, t, f, 1.0f));
			cull_box.Transform(cull_box, XMMatrixInverse(nullptr, V));

			return { V,P };
		}

		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Spot(Light const& light, BoundingFrustum& cull_frustum)
		{
			ADRIA_ASSERT(light.type == ELightType::Spot);

			XMVECTOR light_dir = XMVector3Normalize(light.direction);

			XMVECTOR light_pos = light.position;

			XMVECTOR target_pos = light_pos + light_dir * light.range;

			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMMATRIX V = XMMatrixLookAtLH(light_pos, target_pos, up);

			static const float32 shadow_near = 0.5f;

			float32 fov_angle = 2.0f * acos(light.outer_cosine);

			XMMATRIX P = XMMatrixPerspectiveFovLH(fov_angle, 1.0f, shadow_near, light.range);

			cull_frustum = BoundingFrustum(P);
			cull_frustum.Transform(cull_frustum, XMMatrixInverse(nullptr, V));

			return { V,P };
		}

		std::pair<DirectX::XMMATRIX, DirectX::XMMATRIX> LightViewProjection_Point(Light const& light, uint32 face_index,
			BoundingFrustum& cull_frustum)
		{
			static float32 const shadow_near = 0.5f;
			XMMATRIX P = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, shadow_near, light.range);

			XMVECTOR light_pos = light.position;
			XMMATRIX V{};
			XMVECTOR target{};
			XMVECTOR up{};

			switch (face_index)
			{
			case 0:  //X+
				target = XMVectorAdd(light_pos, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 1:  //X-
				target = XMVectorAdd(light_pos, XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 2:	 //Y+
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 3:  //Y-
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
				up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 4:  //Z+
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			case 5:  //Z-
				target = XMVectorAdd(light_pos, XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f));
				up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
				V = XMMatrixLookAtLH(light_pos, target, up);
				break;
			default:
				ADRIA_ASSERT(false && "Invalid face index!");
			}

			cull_frustum = BoundingFrustum(P);
			cull_frustum.Transform(cull_frustum, XMMatrixInverse(nullptr, V));

			return { V,P };
		}

		std::array<XMMATRIX, CASCADE_COUNT> RecalculateProjectionMatrices(Camera const& camera, float32 split_lambda, std::array<float32, CASCADE_COUNT>& split_distances)
		{

			float32 camera_near = camera.Near();
			float32 camera_far = camera.Far();
			float32 fov = camera.Fov();
			float32 ar = camera.AspectRatio();

			float32 f = 1.0f / CASCADE_COUNT;

			for (uint32 i = 0; i < split_distances.size(); i++)
			{
				float32 fi = (i + 1) * f;
				float32 l = camera_near * pow(camera_far / camera_near, fi);
				float32 u = camera_near + (camera_far - camera_near) * fi;
				split_distances[i] = l * split_lambda + u * (1.0f - split_lambda);
			}

			std::array<XMMATRIX, CASCADE_COUNT> projectionMatrices{};
			projectionMatrices[0] = DirectX::XMMatrixPerspectiveFovLH(fov, ar, camera_near, split_distances[0]);
			for (uint32 i = 1; i < projectionMatrices.size(); ++i)
				projectionMatrices[i] = DirectX::XMMatrixPerspectiveFovLH(fov, ar, split_distances[i - 1], split_distances[i]);

			return projectionMatrices;
		}

		std::pair<DirectX::XMMATRIX, DirectX::XMMATRIX> LightViewProjection_Cascades(Light const& light, Camera const& camera,
			XMMATRIX projection_matrix,
			BoundingBox& cull_box)
		{
			static float32 const farFactor = 1.5f;
			static float32 const lightDistanceFactor = 4.0f;

			//std::array<XMMATRIX, CASCADE_COUNT> projectionMatrices = RecalculateProjectionMatrices(camera);
			std::array<XMMATRIX, CASCADE_COUNT> lightViewProjectionMatrices{};

			//for (u32 i = 0; i < CASCADE_COUNT; ++i)
			//{

			BoundingFrustum frustum(projection_matrix);
			frustum.Transform(frustum, XMMatrixInverse(nullptr, camera.View()));

			///get frustum corners
			std::array<XMFLOAT3, frustum.CORNER_COUNT> corners{};
			frustum.GetCorners(corners.data());

			XMVECTOR frustumCenter = XMVectorSet(0, 0, 0, 0);
			for (uint32 i = 0; i < corners.size(); ++i)
			{
				frustumCenter = frustumCenter + XMLoadFloat3(&corners[i]);
			}
			frustumCenter /= static_cast<float32>(corners.size());

			float32 radius = 0.0f;

			for (uint32 i = 0; i < corners.size(); ++i)
			{
				float32 dist = XMVectorGetX(XMVector3Length(XMLoadFloat3(&corners[i]) - frustumCenter));
				radius = (std::max)(radius, dist);
			}
			radius = std::ceil(radius * 8.0f) / 8.0f;

			XMVECTOR const maxExtents = XMVectorSet(radius, radius, radius, 0);
			XMVECTOR const minExtents = -maxExtents;
			XMVECTOR const cascadeExtents = maxExtents - minExtents;

			XMVECTOR lightDir = light.direction;
			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMMATRIX V = XMMatrixLookAtLH(frustumCenter, frustumCenter + lightDistanceFactor * lightDir * radius, up);

			XMFLOAT3 minE, maxE, cascadeE;

			XMStoreFloat3(&minE, minExtents);
			XMStoreFloat3(&maxE, maxExtents);
			XMStoreFloat3(&cascadeE, cascadeExtents);

			float32 l = minE.x;
			float32 b = minE.y;
			float32 n = minE.z - farFactor * radius;
			float32 r = maxE.x;
			float32 t = maxE.y;
			float32 f = maxE.z * farFactor;

			XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

			XMMATRIX VP = V * P;

			XMFLOAT3 so(0, 0, 0);

			XMVECTOR shadowOrigin = XMLoadFloat3(&so);

			shadowOrigin = XMVector3Transform(shadowOrigin, VP); //sOrigin * P;

			shadowOrigin *= (SHADOW_CASCADE_MAP_SIZE / 2.0f);

			XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);
			XMVECTOR roundOffset = roundedOrigin - shadowOrigin;

			roundOffset *= (2.0f / SHADOW_CASCADE_MAP_SIZE);

			roundOffset *= XMVectorSet(1.0, 1.0, 0.0, 0.0);

			P.r[3] += roundOffset;
			BoundingBox::CreateFromPoints(cull_box, XMVectorSet(l, b, n, 1.0f), XMVectorSet(r, t, f, 1.0f));
			cull_box.Transform(cull_box, XMMatrixInverse(nullptr, V));

			return { V,P };
		}

		//gauss distribution
		
		float32 GaussianDistribution(float32 x, float32 sigma)
		{
			static const float32 square_root_of_two_pi = sqrt(pi_times_2<float32>);
			return expf((-x * x) / (2 * sigma * sigma)) / (sigma * square_root_of_two_pi);
		}
		template<int16 N>
		std::array<float32, 2 * N + 1> GaussKernel(float32 sigma)
		{
			std::array<float32, 2 * N + 1> gauss{};
			float32 sum = 0.0f;
			for (int16 i = -N; i <= N; ++i)
			{
				gauss[i + N] = GaussianDistribution(i * 1.0f, sigma);
				sum += gauss[i + N];
			}
			for (int16 i = -N; i <= N; ++i)
			{
				gauss[i + N] /= sum;
			}

			return gauss;
		}
		
	}
	using namespace shadow_helpers;

	namespace thread_locals
	{
		//transient cbuffers
		thread_local DynamicAllocation object_allocation;
		thread_local DynamicAllocation material_allocation;
		thread_local DynamicAllocation light_allocation;
		thread_local DynamicAllocation shadow_allocation;
		thread_local ObjectCBuffer object_cbuf_data{};		
		thread_local MaterialCBuffer material_cbuf_data{};
		thread_local LightCBuffer light_cbuf_data{};
		thread_local ShadowCBuffer shadow_cbuf_data{};
	}
	using namespace thread_locals;
	using namespace tecs;

	Renderer::Renderer(tecs::registry& reg, GraphicsCoreDX12* gfx, uint32 width, uint32 height)
		: reg(reg), gfx(gfx), width(width), height(height), texture_manager(gfx, 1000), backbuffer_count(gfx->BackbufferCount()),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count), postprocess_cbuffer(gfx->GetDevice(), backbuffer_count),
		compute_cbuffer(gfx->GetDevice(), backbuffer_count), weather_cbuffer(gfx->GetDevice(), backbuffer_count),
		clusters(gfx->GetDevice(), CLUSTER_COUNT, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		light_counter(gfx->GetDevice(), 1, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		light_list(gfx->GetDevice(), CLUSTER_COUNT * CLUSTER_MAX_LIGHTS, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		light_grid(gfx->GetDevice(), CLUSTER_COUNT, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		profiler(gfx), particle_renderer(gfx), picker(gfx)//, ray_tracer(reg, gfx, width, height)
	{
		LoadShaders();
		CreateRootSignatures();
		CreatePipelineStateObjects();
		CreateDescriptorHeaps();

		CreateResolutionDependentResources(width, height);
		CreateOtherResources();
		CreateRenderPasses(width, height);
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

		static float32 _near = 0.0f, far_plane = 0.0f, _fov = 0.0f, _ar = 0.0f;
		if (fabs(_near - camera->Near()) > 1e-4 || fabs(far_plane - camera->Far()) > 1e-4 || fabs(_fov - camera->Fov()) > 1e-4
			|| fabs(_ar - camera->AspectRatio()) > 1e-4)
		{
			_near = camera->Near();
			far_plane = camera->Far();
			_fov = camera->Fov();
			_ar = camera->AspectRatio();
			recreate_clusters = true;
		}
	}
	void Renderer::Update(float32 dt)
	{
		UpdateConstantBuffers(dt);
		CameraFrustumCulling();
		UpdateParticles(dt);
	}

	void Renderer::SetSceneViewportData(SceneViewport&& vp)
	{
		current_scene_viewport = std::move(vp);
	}
	void Renderer::SetProfilerSettings(ProfilerSettings _profiler_settings)
	{
		profiler_settings = _profiler_settings;
	}
	void Renderer::Render(RendererSettings const& _settings)
	{
		settings = _settings;
		if (settings.ibl && !ibl_textures_generated) CreateIBLTextures();
		if (update_picking_data)
		{
			picking_data = picker.GetPickingData();
			update_picking_data = false;
		}

		auto cmd_list = gfx->GetDefaultCommandList();

		ResourceBarrierBatch picker_barrier{};
		depth_target.Transition(picker_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		picker_barrier.Submit(cmd_list);

		PassPicking(cmd_list);

		ResourceBarrierBatch main_barrier{};
		hdr_render_target.Transition(main_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		main_barrier.Submit(cmd_list);

		ResourceBarrierBatch depth_barrier{};
		depth_target.Transition(depth_barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depth_barrier.Submit(cmd_list);

		PassGBuffer(cmd_list);

		depth_barrier.ReverseTransitions();
		depth_barrier.Submit(cmd_list);

		PassDecals(cmd_list);
		if (settings.ambient_occlusion != EAmbientOcclusion::None)
		{
			if(settings.ambient_occlusion == EAmbientOcclusion::SSAO) PassSSAO(cmd_list);
			else if (settings.ambient_occlusion == EAmbientOcclusion::HBAO) PassHBAO(cmd_list);
		}
		depth_barrier.ReverseTransitions();
		depth_barrier.Submit(cmd_list);

		PassAmbient(cmd_list);
		PassDeferredLighting(cmd_list);

		if (settings.use_tiled_deferred) PassDeferredTiledLighting(cmd_list);
		else if (settings.use_clustered_deferred) PassDeferredClusteredLighting(cmd_list);

		PassForward(cmd_list);
		PassParticles(cmd_list);

		main_barrier.Merge(std::move(depth_barrier));
		main_barrier.ReverseTransitions();
		main_barrier.Submit(cmd_list);

		PassPostprocess(cmd_list);
	}
	void Renderer::Render_Multithreaded(RendererSettings const& _settings)
	{
		settings = _settings;
		if (settings.ibl && !ibl_textures_generated) CreateIBLTextures();
		if (update_picking_data)
		{
			picking_data = picker.GetPickingData();
			update_picking_data = false;
		}

		auto cmd_list = gfx->GetDefaultCommandList();
		auto gbuf_cmd_list = gfx->GetNewGraphicsCommandList();
		auto deferred_cmd_list = gfx->GetNewGraphicsCommandList();
		auto postprocess_cmd_list = gfx->GetNewGraphicsCommandList();

		auto gbuf_ambient_fut = TaskSystem::Submit([this, gbuf_cmd_list]()
		{
			D3D12_RESOURCE_BARRIER picking_barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(depth_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				};

			gbuf_cmd_list->ResourceBarrier(ARRAYSIZE(picking_barriers), picking_barriers);
			PassPicking(gbuf_cmd_list);

			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(hdr_render_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
			};

			gbuf_cmd_list->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			PassGBuffer(gbuf_cmd_list);
		}
		);

		auto deferred_fut = TaskSystem::Submit([this, deferred_cmd_list]()
		{
			D3D12_RESOURCE_BARRIER pre_ssao_barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target.Resource(),D3D12_RESOURCE_STATE_DEPTH_WRITE,  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			};
			deferred_cmd_list->ResourceBarrier(ARRAYSIZE(pre_ssao_barriers), pre_ssao_barriers);

			PassDecals(deferred_cmd_list);

			if (settings.ambient_occlusion != EAmbientOcclusion::None)
			{
				if (settings.ambient_occlusion == EAmbientOcclusion::SSAO) PassSSAO(deferred_cmd_list);
				else if (settings.ambient_occlusion == EAmbientOcclusion::HBAO) PassHBAO(deferred_cmd_list);
			}
			D3D12_RESOURCE_BARRIER  post_ssao_barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
			};
			deferred_cmd_list->ResourceBarrier(ARRAYSIZE(post_ssao_barriers), post_ssao_barriers);


			PassAmbient(deferred_cmd_list);
			PassDeferredLighting(deferred_cmd_list);

			if (settings.use_tiled_deferred) PassDeferredTiledLighting(deferred_cmd_list);
			else if (settings.use_clustered_deferred) PassDeferredClusteredLighting(deferred_cmd_list);

			PassForward(deferred_cmd_list);
			PassParticles(deferred_cmd_list);
		}
		);

		auto postprocess_fut = TaskSystem::Submit([this, postprocess_cmd_list]()
		{
			
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(hdr_render_target.Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target.Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			};

			postprocess_cmd_list->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			PassPostprocess(postprocess_cmd_list);
		});

		gbuf_ambient_fut.wait();
		deferred_fut.wait();
		postprocess_fut.wait();
	}
	void Renderer::ResolveToBackbuffer()
	{
		auto cmd_list = gfx->GetLastGraphicsCommandList();
		D3D12_VIEWPORT vp{};
		vp.Width = (float32)width;
		vp.Height = (float32)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		cmd_list->RSSetViewports(1, &vp);
		D3D12_RECT rect{};
		rect.bottom = (int64)height;
		rect.left = 0;
		rect.right = (int64)width;
		rect.top = 0;
		cmd_list->RSSetScissorRects(1, &rect);

		if (settings.anti_aliasing & AntiAliasing_FXAA)
		{
			fxaa_render_pass.Begin(cmd_list);
			PassToneMap(cmd_list);
			fxaa_render_pass.End(cmd_list);
			gfx->SetBackbuffer(cmd_list);
			PassFXAA(cmd_list);
		}
		else
		{
			gfx->SetBackbuffer(cmd_list);
			PassToneMap(cmd_list);
		}

	}
	void Renderer::ResolveToOffscreenFramebuffer()
	{
		auto cmd_list = gfx->GetLastGraphicsCommandList();
		if (settings.anti_aliasing & AntiAliasing_FXAA)
		{
			fxaa_render_pass.Begin(cmd_list);
			PassToneMap(cmd_list);
			fxaa_render_pass.End(cmd_list);

			ResourceBarrierBatch offscreen_barrier{};
			offscreen_ldr_target.Transition(offscreen_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
			offscreen_barrier.Submit(cmd_list);

			offscreen_resolve_pass.Begin(cmd_list);

			PassFXAA(cmd_list);

			offscreen_resolve_pass.End(cmd_list);

			offscreen_barrier.ReverseTransitions();
			offscreen_barrier.Submit(cmd_list);

		}
		else
		{
			ResourceBarrierBatch offscreen_barrier{};
			offscreen_ldr_target.Transition(offscreen_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
			offscreen_barrier.Submit(cmd_list);

			offscreen_resolve_pass.Begin(cmd_list);

			PassToneMap(cmd_list);

			offscreen_resolve_pass.End(cmd_list);

			offscreen_barrier.ReverseTransitions();
			offscreen_barrier.Submit(cmd_list);
		}
	}
	void Renderer::OnResize(uint32 _width, uint32 _height)
	{
		width = _width, height = _height;
		if (width != 0 || height != 0)
		{
			CreateResolutionDependentResources(width, height);
			CreateRenderPasses(width, height);
		}
	}
	void Renderer::OnRightMouseClicked()
	{
		update_picking_data = current_scene_viewport.scene_viewport_focused;
	}
	void Renderer::UploadData()
	{
		//create cube vb and ib for sky here, move somewhere else later
		{

			static const SimpleVertex cube_vertices[8] =
			{
				XMFLOAT3{ -0.5f, -0.5f,  0.5f },
				XMFLOAT3{  0.5f, -0.5f,  0.5f },
				XMFLOAT3{  0.5f,  0.5f,  0.5f },
				XMFLOAT3{ -0.5f,  0.5f,  0.5f },
				XMFLOAT3{ -0.5f, -0.5f, -0.5f },
				XMFLOAT3{  0.5f, -0.5f, -0.5f },
				XMFLOAT3{  0.5f,  0.5f, -0.5f },
				XMFLOAT3{ -0.5f,  0.5f, -0.5f }
			};

			static const uint16_t cube_indices[36] =
			{
				// front
				0, 1, 2,
				2, 3, 0,
				// right
				1, 5, 6,
				6, 2, 1,
				// back
				7, 6, 5,
				5, 4, 7,
				// left
				4, 0, 3,
				3, 7, 4,
				// bottom
				4, 5, 1,
				1, 0, 4,
				// top
				3, 2, 6,
				6, 7, 3
			};

			cube_vb = std::make_shared<VertexBuffer>(gfx, cube_vertices, ARRAYSIZE(cube_vertices));
			cube_ib = std::make_shared<IndexBuffer>(gfx, cube_indices, ARRAYSIZE(cube_indices));
		}

		//ao textures
		ID3D12Resource* ssao_upload_texture = nullptr;
		{
			const uint64 upload_buffer_size = GetRequiredIntermediateSize(ssao_random_texture.Resource(), 0, 1);

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&ssao_upload_texture)));

			RealRandomGenerator rand_float{ 0.0f, 1.0f };
			std::vector<float32> random_texture_data;
			for (int32 i = 0; i < 8 * 8; i++)
			{

				random_texture_data.push_back(rand_float()); //2 * rand_float() - 1
				random_texture_data.push_back(rand_float());
				random_texture_data.push_back(0.0f);
				random_texture_data.push_back(1.0f);
			}

			D3D12_SUBRESOURCE_DATA data{};
			data.pData = random_texture_data.data();
			data.RowPitch = 8 * 4 * sizeof(float32);
			data.SlicePitch = 0;

			UpdateSubresources(gfx->GetDefaultCommandList(), ssao_random_texture.Resource(), ssao_upload_texture, 0, 0, 1, &data);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(ssao_random_texture.Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			gfx->GetDefaultCommandList()->ResourceBarrier(1, &barrier);
		}

		ID3D12Resource* hbao_upload_texture = nullptr;
		{
			const uint64 upload_buffer_size = GetRequiredIntermediateSize(hbao_random_texture.Resource(), 0, 1);

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&hbao_upload_texture)));

			RealRandomGenerator rand_float{ 0.0f, 1.0f };
			std::vector<float32> random_texture_data;
			for (int32 i = 0; i < 8 * 8; i++)
			{
				float32 rand = rand_float();
				random_texture_data.push_back(sin(rand)); 
				random_texture_data.push_back(cos(rand));
				random_texture_data.push_back(rand_float());
				random_texture_data.push_back(rand_float());
			}

			D3D12_SUBRESOURCE_DATA data{};
			data.pData = random_texture_data.data();
			data.RowPitch = 8 * 4 * sizeof(float32);
			data.SlicePitch = 0;

			UpdateSubresources(gfx->GetDefaultCommandList(), hbao_random_texture.Resource(), hbao_upload_texture, 0, 0, 1, &data);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(hbao_random_texture.Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			gfx->GetDefaultCommandList()->ResourceBarrier(1, &barrier);
		}

		//bokeh upload indirect draw buffer and zero counter buffer
		ID3D12Resource* bokeh_upload_buffer = nullptr;
		{
			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.SampleDesc.Count = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Width = 4 * sizeof(uint32);
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&bokeh_indirect_draw_buffer)));

			hex_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Hex.dds");
			oct_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Oct.dds");
			circle_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Circle.dds");
			cross_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Cross.dds");

			const uint64 upload_buffer_size = GetRequiredIntermediateSize(bokeh_indirect_draw_buffer.Get(), 0, 1);
			heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&bokeh_upload_buffer)));

			uint32 init_data[] = { 0,1,0,0 };

			D3D12_SUBRESOURCE_DATA data{};
			data.pData = init_data;
			data.RowPitch = sizeof(init_data);
			data.SlicePitch = 0;

			UpdateSubresources(gfx->GetDefaultCommandList(), bokeh_indirect_draw_buffer.Get(), bokeh_upload_buffer, 0, 0, 1, &data);

			D3D12_INDIRECT_ARGUMENT_DESC args[1];
			args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

			// Data structure to match the command signature used for ExecuteIndirect.

			D3D12_COMMAND_SIGNATURE_DESC command_signature_desc{};
			command_signature_desc.NumArgumentDescs = 1;
			command_signature_desc.pArgumentDescs = args;
			command_signature_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);

			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&bokeh_command_signature)));

			// Allocate a buffer that can be used to reset the UAV counters and initialize it to 0.
			auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32));
			auto upload_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommittedResource(
				&upload_heap,
				D3D12_HEAP_FLAG_NONE,
				&buffer_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&counter_reset_buffer)));

			uint8* mapped_reset_buffer = nullptr;
			CD3DX12_RANGE read_range(0, 0);        // We do not intend to read from this resource on the CPU.
			BREAK_IF_FAILED(counter_reset_buffer->Map(0, &read_range, reinterpret_cast<void**>(&mapped_reset_buffer)));
			ZeroMemory(mapped_reset_buffer, sizeof(uint32));
			counter_reset_buffer->Unmap(0, nullptr);
		}

		//ocean ping phase initial
		ID3D12Resource* ping_phase_upload_buffer = nullptr;
		{
			const uint64 upload_buffer_size = GetRequiredIntermediateSize(ping_pong_phase_textures[pong_phase].Resource(), 0, 1);
			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&ping_phase_upload_buffer)));

			std::vector<float32> ping_array(RESOLUTION * RESOLUTION);
			RealRandomGenerator rand_float{ 0.0f,  2.0f * pi<float32> };
			for (size_t i = 0; i < ping_array.size(); ++i) ping_array[i] = rand_float();

			D3D12_SUBRESOURCE_DATA data{};
			data.pData = ping_array.data();
			data.RowPitch = sizeof(float32) * RESOLUTION;
			data.SlicePitch = 0;

			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = ping_pong_phase_textures[pong_phase].Resource();
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			ID3D12GraphicsCommandList* cmd_list = gfx->GetDefaultCommandList();
			cmd_list->ResourceBarrier(1, &barrier);
			UpdateSubresources(cmd_list, ping_pong_phase_textures[pong_phase].Resource(), ping_phase_upload_buffer, 0, 0, 1, &data);
			std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
			cmd_list->ResourceBarrier(1, &barrier);
		}

		gfx->AddToReleaseQueue(ssao_upload_texture);
		gfx->AddToReleaseQueue(hbao_upload_texture);
		gfx->AddToReleaseQueue(bokeh_upload_buffer);
		gfx->AddToReleaseQueue(ping_phase_upload_buffer);

		particle_renderer.UploadData();

		//lens flare
		texture_manager.SetMipMaps(false);
		{
			TEXTURE_HANDLE tex_handle{};

			ResourceBarrierBatch barrier{};

			tex_handle = texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare0.jpg");
			lens_flare_textures.push_back(texture_manager.CpuDescriptorHandle(tex_handle));

			texture_manager.TransitionTexture(tex_handle, barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			tex_handle = texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare1.jpg");
			lens_flare_textures.push_back(texture_manager.CpuDescriptorHandle(tex_handle));

			texture_manager.TransitionTexture(tex_handle, barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			tex_handle = texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare2.jpg");
			lens_flare_textures.push_back(texture_manager.CpuDescriptorHandle(tex_handle));

			texture_manager.TransitionTexture(tex_handle, barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			tex_handle = texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare3.jpg");
			lens_flare_textures.push_back(texture_manager.CpuDescriptorHandle(tex_handle));

			texture_manager.TransitionTexture(tex_handle, barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			tex_handle = texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare4.jpg");
			lens_flare_textures.push_back(texture_manager.CpuDescriptorHandle(tex_handle));

			texture_manager.TransitionTexture(tex_handle, barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			tex_handle = texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare5.jpg");
			lens_flare_textures.push_back(texture_manager.CpuDescriptorHandle(tex_handle));

			texture_manager.TransitionTexture(tex_handle, barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			tex_handle = texture_manager.LoadTexture(L"Resources/Textures/lensflare/flare6.jpg");
			lens_flare_textures.push_back(texture_manager.CpuDescriptorHandle(tex_handle));

			texture_manager.TransitionTexture(tex_handle, barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			barrier.Submit(gfx->GetDefaultCommandList());
		}

		//clouds and ocean
		{
			clouds_textures.push_back(texture_manager.CpuDescriptorHandle(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\weather.dds")));
			clouds_textures.push_back(texture_manager.CpuDescriptorHandle(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds")));
			clouds_textures.push_back(texture_manager.CpuDescriptorHandle(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\worley.dds")));

			foam_handle = texture_manager.LoadTexture(L"Resources/Textures/foam.jpg");
			perlin_handle = texture_manager.LoadTexture(L"Resources/Textures/perlin.dds");
		}

		texture_manager.SetMipMaps(true);

		//if(ray_tracer.IsSupported()) ray_tracer.BuildAccelerationStructures();
	}
	TextureManager& Renderer::GetTextureManager()
	{
		return texture_manager;
	}
	std::vector<std::string> Renderer::GetProfilerResults(bool log)
{
		return profiler.GetProfilerResults(gfx->GetDefaultCommandList(), log);
	}
	PickingData Renderer::GetPickingData() const
	{
		return picking_data;
	}
	Texture2D Renderer::GetOffscreenTexture() const
	{
		return offscreen_ldr_target;
	}

	void Renderer::LoadShaders()
	{
		//misc
		{
			ShaderBlob vs_blob, ps_blob, cs_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SkyboxVS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SkyboxPS.cso", ps_blob);
			shader_map[VS_Skybox] = vs_blob;
			shader_map[PS_Skybox] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/UniformColorSkyPS.cso", ps_blob);
			shader_map[PS_UniformColorSky] = ps_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/HosekWilkieSkyPS.cso", ps_blob);
			shader_map[PS_HosekWilkieSky] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TextureVS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TexturePS.cso", ps_blob);
			shader_map[VS_Texture] = vs_blob;
			shader_map[PS_Texture] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SunVS.cso", vs_blob);
			shader_map[VS_Sun] = vs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BillboardVS.cso", vs_blob);
			shader_map[VS_Billboard] = vs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/DecalVS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/DecalPS.cso", ps_blob);
			shader_map[VS_Decals] = vs_blob;
			shader_map[PS_Decals] = ps_blob;

			ShaderInfo shader_info_ps{};
			shader_info_ps.shadersource = "Resources/Shaders/Misc/DecalPS.hlsl";
			shader_info_ps.stage = ShaderStage::PS;
			shader_info_ps.defines = { {L"DECAL_MODIFY_NORMALS", L""}};
			shader_info_ps.entrypoint = "main";
#ifdef _DEBUG
			shader_info_ps.flags = ShaderInfo::FLAG_DEBUG | ShaderInfo::FLAG_DISABLE_OPTIMIZATION;
#else 
			shader_info_ps.flags = ShaderInfo::FLAG_NONE;
#endif
			ShaderUtility::CompileShader(shader_info_ps, ps_blob);
			shader_map[PS_Decals_ModifyNormals] = ps_blob;
		}

		//postprocess
		{
			ShaderBlob vs_blob, ps_blob, gs_blob;

			
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ScreenQuadVS.cso", vs_blob);
			shader_map[VS_ScreenQuad] = vs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SSAO_PS.cso", ps_blob);
			shader_map[PS_Ssao] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/HBAO_PS.cso", ps_blob);
			shader_map[PS_Hbao] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SSR_PS.cso", ps_blob);
			shader_map[PS_Ssr] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GodRaysPS.cso", ps_blob);
			shader_map[PS_GodRays] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/CloudsPS.cso", ps_blob);
			shader_map[PS_VolumetricClouds] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ToneMapPS.cso", ps_blob);
			shader_map[PS_ToneMap] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FXAA_PS.cso", ps_blob);
			shader_map[PS_Fxaa] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TAA_PS.cso", ps_blob);
			shader_map[PS_Taa] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/CopyPS.cso", ps_blob);
			shader_map[PS_Copy] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/AddPS.cso", ps_blob);
			shader_map[PS_Add] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LensFlareVS.cso", vs_blob);
			shader_map[VS_LensFlare] = vs_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LensFlareGS.cso", gs_blob);
			shader_map[GS_LensFlare] = gs_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LensFlarePS.cso", ps_blob);
			shader_map[PS_LensFlare] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehVS.cso", vs_blob);
			shader_map[VS_Bokeh] = vs_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehGS.cso", gs_blob);
			shader_map[GS_Bokeh] = gs_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehPS.cso", ps_blob);
			shader_map[PS_Bokeh] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/DOF_PS.cso", ps_blob);
			shader_map[PS_Dof] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/MotionBlurPS.cso", ps_blob);
			shader_map[PS_MotionBlur] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FogPS.cso", ps_blob);
			shader_map[PS_Fog] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VelocityBufferPS.cso", ps_blob);
			shader_map[PS_VelocityBuffer] = ps_blob;
		}

		//gbuffer 
		{
			ShaderBlob vs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GeometryPassPBR_VS.cso", vs_blob);
			shader_map[VS_GBufferPBR] = vs_blob;

			ShaderBlob ps_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GeometryPassPBR_PS.cso", ps_blob);
			shader_map[PS_GBufferPBR] = ps_blob;
		}

		//ambient & lighting
		{
			ShaderBlob ps_blob;
			ShaderInfo shader_info_ps{};
			shader_info_ps.shadersource = "Resources/Shaders/Deferred/AmbientPBR_PS.hlsl";
			shader_info_ps.stage = ShaderStage::PS;
			shader_info_ps.defines = {};

			ShaderUtility::CompileShader(shader_info_ps, ps_blob);
			shader_map[PS_AmbientPBR] = ps_blob;

			shader_info_ps.defines.emplace_back(L"SSAO", L"1");
			ShaderUtility::CompileShader(shader_info_ps, ps_blob);
			shader_map[PS_AmbientPBR_AO] = ps_blob;

			shader_info_ps.defines.clear();
			shader_info_ps.defines.emplace_back(L"IBL", L"1");
			ShaderUtility::CompileShader(shader_info_ps, ps_blob);
			shader_map[PS_AmbientPBR_IBL] = ps_blob;

			shader_info_ps.defines.emplace_back(L"SSAO", L"1");
			ShaderUtility::CompileShader(shader_info_ps, ps_blob);
			shader_map[PS_AmbientPBR_AO_IBL] = ps_blob;
			
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/LightingPBR_PS.cso", ps_blob);
			shader_map[PS_LightingPBR] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterLightingPBR_PS.cso", ps_blob);
			shader_map[PS_ClusteredLightingPBR] = ps_blob;
		}

		//depth maps
		{
			ShaderBlob vs_blob, ps_blob;
			ShaderInfo vs_input{}, ps_input{};
			vs_input.shadersource = "Resources/Shaders/Shadows/DepthMapVS.hlsl";
			vs_input.stage = ShaderStage::VS;
			
			ps_input.shadersource = "Resources/Shaders/Shadows/DepthMapPS.hlsl";
			ps_input.stage = ShaderStage::PS;
			
			ShaderUtility::CompileShader(vs_input, vs_blob);
			ShaderUtility::CompileShader(ps_input, ps_blob);
			
			//eDepthMap_Transparent

			shader_map[VS_DepthMap] = vs_blob;
			shader_map[PS_DepthMap] = ps_blob;

			vs_input.defines.emplace_back(L"TRANSPARENT", L"1");
			ps_input.defines.emplace_back(L"TRANSPARENT", L"1");

			
			ShaderUtility::CompileShader(vs_input, vs_blob);
			ShaderUtility::CompileShader(ps_input, ps_blob);
			
			shader_map[VS_DepthMap_Transparent] = vs_blob;
			shader_map[PS_DepthMap_Transparent] = ps_blob;
		}

		//volumetric
		{
			ShaderBlob ps_blob;
			
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightDirectionalPS.cso", ps_blob);
			shader_map[PS_Volumetric_Directional] = ps_blob;
			
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightDirectionalCascadesPS.cso", ps_blob);
			shader_map[PS_Volumetric_DirectionalCascades] = ps_blob;
			
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightSpotPS.cso", ps_blob);
			shader_map[PS_Volumetric_Spot] = ps_blob;
			
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/VolumetricLightPointPS.cso", ps_blob);
			shader_map[PS_Volumetric_Point] = ps_blob;

		}

		//compute shaders
		{
			ShaderBlob cs_blob;
			ShaderInfo blur_shader{};

			blur_shader.shadersource = "Resources/Shaders/PostProcess/BlurCS.hlsl";
			blur_shader.stage = ShaderStage::CS;

			ShaderUtility::CompileShader(blur_shader, cs_blob);
			shader_map[CS_Blur_Horizontal] = cs_blob;

			blur_shader.defines.emplace_back(L"VERTICAL", L"1");
			ShaderUtility::CompileShader(blur_shader, cs_blob);
			shader_map[CS_Blur_Vertical] = cs_blob;

			blur_shader.shadersource = "Resources/Shaders/Misc/GenerateMipsCS.hlsl";
			ShaderUtility::CompileShader(blur_shader, cs_blob);
			shader_map[CS_GenerateMips] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BloomExtractCS.cso", cs_blob);
			shader_map[CS_BloomExtract] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BloomCombineCS.cso", cs_blob);
			shader_map[CS_BloomCombine] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TiledLightingCS.cso", cs_blob);
			shader_map[CS_TiledLighting] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterBuildingCS.cso", cs_blob);
			shader_map[CS_ClusterBuilding] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterCullingCS.cso", cs_blob);
			shader_map[CS_ClusterCulling] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehCS.cso", cs_blob);
			shader_map[CS_BokehGenerate] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FFT_horizontalCS.cso", cs_blob);
			shader_map[CS_FFT_Horizontal] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/FFT_verticalCS.cso", cs_blob);
			shader_map[CS_FFT_Vertical] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/InitialSpectrumCS.cso", cs_blob);
			shader_map[CS_InitialSpectrum] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/NormalMapCS.cso", cs_blob);
			shader_map[CS_OceanNormalMap] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/PhaseCS.cso", cs_blob);
			shader_map[CS_Phase] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SpectrumCS.cso", cs_blob);
			shader_map[CS_Spectrum] = cs_blob;
		}

		//ocean
		{
			ShaderBlob vs_blob, ps_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanVS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanPS.cso", ps_blob);
			shader_map[VS_Ocean] = vs_blob;
			shader_map[PS_Ocean] = ps_blob;

			ShaderBlob hs_blob, ds_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanLodVS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanLodHS.cso", hs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/OceanLodDS.cso", ds_blob);
			shader_map[VS_OceanLOD] = vs_blob;
			shader_map[HS_OceanLOD] = hs_blob;
			shader_map[DS_OceanLOD] = ds_blob;
		}
	}
	void Renderer::CreateRootSignatures()
	{
		ID3D12Device* device = gfx->GetDevice();
		//in HLSL
		{
			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Skybox].GetPointer(), shader_map[PS_Skybox].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Skybox].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_HosekWilkieSky].GetPointer(), shader_map[PS_HosekWilkieSky].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Sky].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_ToneMap].GetPointer(), shader_map[PS_ToneMap].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::ToneMap].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Fxaa].GetPointer(), shader_map[PS_Fxaa].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::FXAA].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Taa].GetPointer(), shader_map[PS_Taa].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::TAA].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_GBufferPBR].GetPointer(), shader_map[PS_GBufferPBR].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::GbufferPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_AmbientPBR].GetPointer(), shader_map[PS_AmbientPBR].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::AmbientPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_LightingPBR].GetPointer(), shader_map[PS_LightingPBR].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::LightingPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_ClusteredLightingPBR].GetPointer(), shader_map[PS_ClusteredLightingPBR].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::ClusteredLightingPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_DepthMap].GetPointer(), shader_map[PS_DepthMap].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::DepthMap].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_DepthMap_Transparent].GetPointer(), shader_map[PS_DepthMap_Transparent].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::DepthMap_Transparent].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Volumetric_Directional].GetPointer(), shader_map[PS_Volumetric_Directional].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Volumetric].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Texture].GetPointer(), shader_map[PS_Texture].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Forward].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Ssr].GetPointer(), shader_map[PS_Ssr].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::SSR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_GodRays].GetPointer(), shader_map[PS_GodRays].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::GodRays].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_LensFlare].GetPointer(), shader_map[PS_LensFlare].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::LensFlare].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Dof].GetPointer(), shader_map[PS_Dof].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::DOF].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Add].GetPointer(), shader_map[PS_Add].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Add].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Fog].GetPointer(), shader_map[PS_Fog].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Fog].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_VolumetricClouds].GetPointer(), shader_map[PS_VolumetricClouds].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Clouds].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_MotionBlur].GetPointer(), shader_map[PS_MotionBlur].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::MotionBlur].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Bokeh].GetPointer(), shader_map[PS_Bokeh].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Bokeh].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_TiledLighting].GetPointer(), shader_map[CS_TiledLighting].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::TiledLighting].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ClusterBuilding].GetPointer(), shader_map[CS_ClusterBuilding].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::ClusterBuilding].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ClusterCulling].GetPointer(), shader_map[CS_ClusterCulling].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::ClusterCulling].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_BokehGenerate].GetPointer(), shader_map[CS_BokehGenerate].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::BokehGenerate].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_VelocityBuffer].GetPointer(), shader_map[PS_VelocityBuffer].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::VelocityBuffer].GetAddressOf())));

			rs_map[ERootSignature::Copy] = rs_map[ERootSignature::FXAA];

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_FFT_Horizontal].GetPointer(), shader_map[CS_FFT_Horizontal].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::FFT].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_InitialSpectrum].GetPointer(), shader_map[CS_InitialSpectrum].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::InitialSpectrum].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_OceanNormalMap].GetPointer(), shader_map[CS_OceanNormalMap].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::OceanNormalMap].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_Phase].GetPointer(), shader_map[CS_Phase].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Phase].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_Spectrum].GetPointer(), shader_map[CS_Spectrum].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Spectrum].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[VS_Ocean].GetPointer(), shader_map[VS_Ocean].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Ocean].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[VS_OceanLOD].GetPointer(), shader_map[VS_OceanLOD].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::OceanLOD].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Decals].GetPointer(), shader_map[PS_Decals].GetLength(),
				IID_PPV_ARGS(rs_map[ERootSignature::Decals].GetAddressOf())));

			//ID3D12VersionedRootSignatureDeserializer* drs = nullptr;
			//D3D12CreateVersionedRootSignatureDeserializer(shader_map[PS_Add].GetPointer(), shader_map[PS_Add].GetLength(), IID_PPV_ARGS(&drs));
			//D3D12_VERSIONED_ROOT_SIGNATURE_DESC const* desc = drs->GetUnconvertedRootSignatureDesc();
		}

		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		//in C++
		{
			//ao
			{
				std::array<CD3DX12_ROOT_PARAMETER1, 3> root_parameters{};
				CD3DX12_ROOT_PARAMETER1 root_parameter{};

				root_parameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
				root_parameters[1].InitAsConstantBufferView(5, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

				CD3DX12_DESCRIPTOR_RANGE1 range{};
				range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1, 0);
				root_parameters[2].InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);

				std::array<D3D12_STATIC_SAMPLER_DESC, 2> samplers{};
				D3D12_STATIC_SAMPLER_DESC sampler{};
				sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				sampler.MipLODBias = 0;
				sampler.MaxAnisotropy = 0;
				sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
				sampler.MinLOD = 0.0f;
				sampler.MaxLOD = D3D12_FLOAT32_MAX;
				sampler.ShaderRegister = 1;
				sampler.RegisterSpace = 0;
				sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				samplers[0] = sampler;

				sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.ShaderRegister = 0;
				samplers[1] = sampler;

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
				rootSignatureDesc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), (uint32)samplers.size(), samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, feature_data.HighestVersion, &signature, &error);
				if (error) OutputDebugStringA((char*)error->GetBufferPointer());
				BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::AO])));
			}

			//blur
			{
				std::array<CD3DX12_ROOT_PARAMETER1, 3> root_parameters{};
				CD3DX12_ROOT_PARAMETER1 root_parameter{};

				root_parameters[0].InitAsConstantBufferView(6, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_DESCRIPTOR_RANGE1 srv_range{};
				srv_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
				root_parameters[1].InitAsDescriptorTable(1, &srv_range, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_DESCRIPTOR_RANGE1 uav_range{};
				uav_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
				root_parameters[2].InitAsDescriptorTable(1, &uav_range, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
				rootSignatureDesc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, feature_data.HighestVersion, &signature, &error);
				if (error) OutputDebugStringA((char*)error->GetBufferPointer());

				BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::Blur])));
			}

			//bloom 
			{
				rs_map[ERootSignature::BloomExtract] = rs_map[ERootSignature::Blur];

				std::array<CD3DX12_ROOT_PARAMETER1, 2> root_parameters{};
				CD3DX12_ROOT_PARAMETER1 root_parameter{};

				CD3DX12_DESCRIPTOR_RANGE1 srv_range{};
				srv_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);
				root_parameters[0].InitAsDescriptorTable(1, &srv_range, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_DESCRIPTOR_RANGE1 uav_range{};
				uav_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
				root_parameters[1].InitAsDescriptorTable(1, &uav_range, D3D12_SHADER_VISIBILITY_ALL);

				D3D12_STATIC_SAMPLER_DESC linear_clamp_sampler = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
					D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
				root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 1, &linear_clamp_sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				HRESULT hr = D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
				if (error) OutputDebugStringA((char*)error->GetBufferPointer());

				BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::BloomCombine])));
			}

			//mips
			{
				D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};

				feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

				if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
				{
					feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
				}

				CD3DX12_DESCRIPTOR_RANGE1 srv_uav_ranges[2] = {};
				CD3DX12_ROOT_PARAMETER1 root_parameters[3] = {};
				srv_uav_ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
				srv_uav_ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
				root_parameters[0].InitAsConstants(2, 0);
				root_parameters[1].InitAsDescriptorTable(1, &srv_uav_ranges[0]);
				root_parameters[2].InitAsDescriptorTable(1, &srv_uav_ranges[1]);

				D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
					D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

				D3D12_STATIC_SAMPLER_DESC sampler = {};
				sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				sampler.MipLODBias = 0.0f;
				sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				sampler.MinLOD = 0.0f;
				sampler.MaxLOD = D3D12_FLOAT32_MAX;
				sampler.MaxAnisotropy = 0;
				sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
				sampler.ShaderRegister = 0;
				sampler.RegisterSpace = 0;
				sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
				root_signature_desc.Init_1_1(ARRAYSIZE(root_parameters), root_parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				Microsoft::WRL::ComPtr<ID3DBlob> signature;
				Microsoft::WRL::ComPtr<ID3DBlob> error;

				BREAK_IF_FAILED(D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error));
				BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[ERootSignature::GenerateMips])));
			}
		}
	}
#pragma warning( push )
#pragma warning( disable : 6262 )
	void Renderer::CreatePipelineStateObjects()
	{
		ID3D12Device* device = gfx->GetDevice();
		{
			//skybox
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Skybox], input_layout);
				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[ERootSignature::Skybox].Get();
				pso_desc.VS = shader_map[VS_Skybox];
				pso_desc.PS = shader_map[PS_Skybox];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.DepthStencilState.DepthEnable = TRUE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Skybox])));

				pso_desc.pRootSignature = rs_map[ERootSignature::Sky].Get();
				pso_desc.PS = shader_map[PS_UniformColorSky];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::UniformColorSky])));

				pso_desc.PS = shader_map[PS_HosekWilkieSky];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::HosekWilkieSky])));
			}

			//tonemap
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);
				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[ERootSignature::ToneMap].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_ToneMap];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ToneMap])));

			}

			//fxaa
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);

				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[ERootSignature::FXAA].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Fxaa];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R10G10B10A2_UNORM;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FXAA])));
			}

			//taa
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);

				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[ERootSignature::TAA].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Taa];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::TAA])));
			}

			//gbuffer
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_GBufferPBR], input_layout);

				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[ERootSignature::GbufferPBR].Get();
				pso_desc.VS = shader_map[VS_GBufferPBR];
				pso_desc.PS = shader_map[PS_GBufferPBR];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.DepthStencilState.DepthEnable = TRUE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = static_cast<uint32>(3);
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GbufferPBR])));
			}

			//ambient 
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = { nullptr, 0u };
				pso_desc.pRootSignature = rs_map[ERootSignature::AmbientPBR].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_AmbientPBR];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

				pso_desc.DepthStencilState.DepthEnable = FALSE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR])));

				pso_desc.PS = shader_map[PS_AmbientPBR_AO];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_AO])));

				pso_desc.PS = shader_map[PS_AmbientPBR_IBL];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_IBL])));

				pso_desc.PS = shader_map[PS_AmbientPBR_AO_IBL];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::AmbientPBR_AO_IBL])));

			}

			//lighting & clustered lighting
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = { nullptr, 0u };
				pso_desc.pRootSignature = rs_map[ERootSignature::LightingPBR].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_LightingPBR];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				pso_desc.DepthStencilState.DepthEnable = FALSE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LightingPBR])));


				pso_desc.pRootSignature = rs_map[ERootSignature::ClusteredLightingPBR].Get();
				pso_desc.PS = shader_map[PS_ClusteredLightingPBR];

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusteredLightingPBR])));
			}

			//clustered building and culling
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[ERootSignature::ClusterBuilding].Get();
				pso_desc.CS = shader_map[CS_ClusterBuilding];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusterBuilding])));

				pso_desc.pRootSignature = rs_map[ERootSignature::ClusterCulling].Get();
				pso_desc.CS = shader_map[CS_ClusterCulling];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::ClusterCulling])));
			}

			//tiled lighting
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[ERootSignature::TiledLighting].Get();
				pso_desc.CS = shader_map[CS_TiledLighting];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::TiledLighting])));
			}

			//depth maps
			{
				InputLayout il;

				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_DepthMap], il);

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[ERootSignature::DepthMap].Get();
				pso_desc.VS = shader_map[VS_DepthMap];
				pso_desc.PS = shader_map[PS_DepthMap];
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
				pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				pso_desc.RasterizerState.DepthBias = 7500;
				pso_desc.RasterizerState.DepthBiasClamp = 0.0f;
				pso_desc.RasterizerState.SlopeScaledDepthBias = 1.0f;

				pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				pso_desc.DepthStencilState.DepthEnable = TRUE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;

				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 0;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DepthMap])));

				//InputLayout il2;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_DepthMap_Transparent], il);
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[ERootSignature::DepthMap_Transparent].Get();
				pso_desc.VS = shader_map[VS_DepthMap_Transparent];
				pso_desc.PS = shader_map[PS_DepthMap_Transparent];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DepthMap_Transparent])));

			}

			//volumetric lighting
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = { nullptr, 0u };
				pso_desc.pRootSignature = rs_map[ERootSignature::Volumetric].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Volumetric_Directional];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

				pso_desc.DepthStencilState.DepthEnable = FALSE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Directional])));

				pso_desc.PS = shader_map[PS_Volumetric_DirectionalCascades];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_DirectionalCascades])));

				pso_desc.PS = shader_map[PS_Volumetric_Spot];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Spot])));

				pso_desc.PS = shader_map[PS_Volumetric_Point];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Volumetric_Point])));

			}
			
			//forward
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Sun], input_layout);
				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[ERootSignature::Forward].Get();
				pso_desc.VS = shader_map[VS_Sun];
				pso_desc.PS = shader_map[PS_Texture];

				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

				pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				pso_desc.DepthStencilState.DepthEnable = TRUE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Sun])));

			}

			//ao
			{

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::AO].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Ssao];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::SSAO])));

				pso_desc.PS = shader_map[PS_Hbao];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::HBAO])));

			}

			//ssr
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::SSR].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Ssr];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::SSR])));
			}

			//god rays
			{
				InputLayout il;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], il);

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[ERootSignature::GodRays].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_GodRays];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GodRays])));
			}

			//lens flare
			{
				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr,0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::LensFlare].Get();
				pso_desc.VS = shader_map[VS_LensFlare];
				pso_desc.GS = shader_map[GS_LensFlare];
				pso_desc.PS = shader_map[PS_LensFlare];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::LensFlare])));
			}

			//blur
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[ERootSignature::Blur].Get();
				pso_desc.CS = shader_map[CS_Blur_Horizontal];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Blur_Horizontal])));

				pso_desc.CS = shader_map[CS_Blur_Vertical];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Blur_Vertical])));
			}

			//bloom extract
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[ERootSignature::BloomExtract].Get();
				pso_desc.CS = shader_map[CS_BloomExtract];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BloomExtract])));
			}

			//bloom combine
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[ERootSignature::BloomCombine].Get();
				pso_desc.CS = shader_map[CS_BloomCombine];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BloomCombine])));
			}

			//mips
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[ERootSignature::GenerateMips].Get();
				pso_desc.CS = shader_map[CS_GenerateMips];
				device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::GenerateMips]));
			}

			//copy
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::Copy].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Copy];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy])));

				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy_AlphaBlend])));


				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Copy_AdditiveBlend])));
			}

			//add
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};

				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::Add].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Add];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add])));

				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add_AlphaBlend])));


				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Add_AdditiveBlend])));
			}

			//fog
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};

				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::Fog].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Fog];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Fog])));

			}

			//dof
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::DOF].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_Dof];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::DOF])));
			}

			//bokeh 
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc = {};
				compute_pso_desc.pRootSignature = rs_map[ERootSignature::BokehGenerate].Get();
				compute_pso_desc.CS = shader_map[CS_BokehGenerate];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::BokehGenerate])));

				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pso_desc{};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[ERootSignature::Bokeh].Get();
				graphics_pso_desc.VS = shader_map[VS_Bokeh];
				graphics_pso_desc.GS = shader_map[GS_Bokeh];
				graphics_pso_desc.PS = shader_map[PS_Bokeh];
				graphics_pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				graphics_pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				graphics_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				graphics_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
				graphics_pso_desc.SampleMask = UINT_MAX;
				graphics_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				graphics_pso_desc.NumRenderTargets = 1;
				graphics_pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				graphics_pso_desc.SampleDesc.Count = 1;
				graphics_pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Bokeh])));
			}

			//clouds
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::Clouds].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_VolumetricClouds];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Clouds])));
			}

			//motion blur
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::MotionBlur].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_MotionBlur];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::MotionBlur])));
			}

			//velocity buffer
			{
				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[ERootSignature::VelocityBuffer].Get();
				pso_desc.VS = shader_map[VS_ScreenQuad];
				pso_desc.PS = shader_map[PS_VelocityBuffer];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::VelocityBuffer])));
			}

			//ocean 
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc = {};
				compute_pso_desc.pRootSignature = rs_map[ERootSignature::FFT].Get();
				compute_pso_desc.CS = shader_map[CS_FFT_Horizontal];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FFT_Horizontal])));

				compute_pso_desc.CS = shader_map[CS_FFT_Vertical];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::FFT_Vertical])));

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::InitialSpectrum].Get();
				compute_pso_desc.CS = shader_map[CS_InitialSpectrum];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::InitialSpectrum])));

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::InitialSpectrum].Get();
				compute_pso_desc.CS = shader_map[CS_InitialSpectrum];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::InitialSpectrum])));

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::OceanNormalMap].Get();
				compute_pso_desc.CS = shader_map[CS_OceanNormalMap];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanNormalMap])));

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Phase].Get();
				compute_pso_desc.CS = shader_map[CS_Phase];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Phase])));

				compute_pso_desc.pRootSignature = rs_map[ERootSignature::Spectrum].Get();
				compute_pso_desc.CS = shader_map[CS_Spectrum];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Spectrum])));

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				InputLayout il;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Ocean], il);
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[ERootSignature::Ocean].Get();
				pso_desc.VS = shader_map[VS_Ocean];
				pso_desc.PS = shader_map[PS_Ocean];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.DepthStencilState.DepthEnable = TRUE;
				pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso_desc.DepthStencilState.StencilEnable = FALSE;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Ocean])));
				pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Ocean_Wireframe])));

				pso_desc.pRootSignature = rs_map[ERootSignature::OceanLOD].Get();
				pso_desc.VS = shader_map[VS_OceanLOD];
				pso_desc.DS = shader_map[DS_OceanLOD];
				pso_desc.HS = shader_map[HS_OceanLOD];
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanLOD_Wireframe])));
				pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::OceanLOD])));
			}

			//decals
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				InputLayout il;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Decals], il);
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[ERootSignature::Decals].Get();
				pso_desc.VS = shader_map[VS_Decals];
				pso_desc.PS = shader_map[PS_Decals];
				pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Decals])));

				pso_desc.PS = shader_map[PS_Decals_ModifyNormals];
				pso_desc.NumRenderTargets = 2;
				pso_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[EPipelineStateObject::Decals_ModifyNormals])));
			}
		}
	}
#pragma warning( pop ) 

	void Renderer::CreateDescriptorHeaps()
	{
		ID3D12Device* device = gfx->GetDevice();

		rtv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
		srv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 50));
		dsv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
		uav_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
		constant_srv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 50));
		constant_dsv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
		constant_uav_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
		null_srv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, NULL_HEAP_SIZE));

		D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
		null_srv_desc.Texture2D.MostDetailedMip = 0;
		null_srv_desc.Texture2D.MipLevels = -1;
		null_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		null_srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_srv_heap->GetCpuHandle(TEXTURE2D_SLOT));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_srv_heap->GetCpuHandle(TEXTURE2DARRAY_SLOT));

		D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc{};
		null_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		null_uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		device->CreateUnorderedAccessView(nullptr,nullptr, &null_uav_desc, null_srv_heap->GetCpuHandle(RWTEXTURE2D_SLOT));

	}
	void Renderer::CreateResolutionDependentResources(uint32 width, uint32 height)
	{
		
		srv_heap_index = 0;
		rtv_heap_index = 0;
		dsv_heap_index = 0;
		uav_heap_index = 0;

		//main render target
		{

			texture2d_desc_t render_target_desc{};
			render_target_desc.clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			render_target_desc.clear_value.Color[0] = 0.0f;
			render_target_desc.clear_value.Color[1] = 0.0f;
			render_target_desc.clear_value.Color[2] = 0.0f;
			render_target_desc.clear_value.Color[3] = 0.0f;
			render_target_desc.width = width;
			render_target_desc.height = height;
			render_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			render_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			hdr_render_target = Texture2D(gfx->GetDevice(), render_target_desc);
			hdr_render_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			hdr_render_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));

			render_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			prev_hdr_render_target = Texture2D(gfx->GetDevice(), render_target_desc);
			prev_hdr_render_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			prev_hdr_render_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));

			sun_target = Texture2D(gfx->GetDevice(), render_target_desc);
			sun_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			sun_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
		}
		
		//depth stencil target
		{


			texture2d_desc_t depth_target_desc{};
			depth_target_desc.width = width;
			depth_target_desc.height = height;
			depth_target_desc.format = DXGI_FORMAT_R32_TYPELESS;
			depth_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			depth_target_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
			depth_target_desc.clear_value.DepthStencil = { 1.0f, 0 };

			depth_target = Texture2D(gfx->GetDevice(), depth_target_desc);

			texture2d_srv_desc_t srv_desc{};
			srv_desc.format = DXGI_FORMAT_R32_FLOAT;
			depth_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++), &srv_desc);
			texture2d_dsv_desc_t dsv_desc{};
			dsv_desc.format = DXGI_FORMAT_D32_FLOAT;
			depth_target.CreateDSV(dsv_heap->GetCpuHandle(dsv_heap_index++), &dsv_desc);
		}

		//low dynamic range render target
		{
			texture2d_desc_t ldr_target_desc{};
			ldr_target_desc.clear_value.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			ldr_target_desc.clear_value.Color[0] = 0.0f;
			ldr_target_desc.clear_value.Color[1] = 0.0f;
			ldr_target_desc.clear_value.Color[2] = 0.0f;
			ldr_target_desc.clear_value.Color[3] = 0.0f;
			ldr_target_desc.width = width;
			ldr_target_desc.height = height;
			ldr_target_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
			ldr_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			ldr_target_desc.start_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
			ldr_render_target = Texture2D(gfx->GetDevice(), ldr_target_desc);

			ldr_render_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			ldr_render_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
		}

		//gbuffer
		{
			gbuffer.clear();

			texture2d_desc_t render_target_desc{};
			render_target_desc.width = width;
			render_target_desc.height = height;
			render_target_desc.clear_value.Color[0] = 0.0f;
			render_target_desc.clear_value.Color[1] = 0.0f;
			render_target_desc.clear_value.Color[2] = 0.0f;
			render_target_desc.clear_value.Color[3] = 0.0f;
			render_target_desc.clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			render_target_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			render_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			render_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			for (uint32 i = 0; i < GBUFFER_SIZE; ++i)
			{
				gbuffer.emplace_back(gfx->GetDevice(), render_target_desc);

				gbuffer.back().CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
				gbuffer.back().CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			}

		}

		//ao
		{
			//ssao texture
			texture2d_desc_t ssao_target_desc{};
			ssao_target_desc.width = width;
			ssao_target_desc.height = height;
			ssao_target_desc.clear_value.Color[0] = 0.0f;
			ssao_target_desc.clear_value.Format = DXGI_FORMAT_R8_UNORM;
			ssao_target_desc.format = DXGI_FORMAT_R8_UNORM;
			ssao_target_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; //its getting blurred in CS
			ssao_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			ao_texture = Texture2D(gfx->GetDevice(), ssao_target_desc);

			ao_texture.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			ao_texture.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
		}

		//blur
		{
			texture2d_desc_t blur_desc{};
			blur_desc.width = width;
			blur_desc.height = height;
			blur_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			blur_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			blur_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; //t0 in CS
			blur_desc.clear_value.Format = blur_desc.format;

			blur_intermediate_texture = Texture2D(gfx->GetDevice(), blur_desc);

			blur_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			blur_final_texture = Texture2D(gfx->GetDevice(), blur_desc);

			blur_intermediate_texture.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			blur_final_texture.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));

			blur_intermediate_texture.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
			blur_final_texture.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
		}

		//bloom
		{
			texture2d_desc_t bloom_extract_desc{};
			bloom_extract_desc.width = width;
			bloom_extract_desc.height = height;
			bloom_extract_desc.mips = 5;
			bloom_extract_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			bloom_extract_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			bloom_extract_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; //t0 in CS
			bloom_extract_desc.clear_value.Format = bloom_extract_desc.format;

			bloom_extract_texture = Texture2D(gfx->GetDevice(), bloom_extract_desc);

			bloom_extract_texture.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			
			bloom_extract_texture.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
		}

		//postprocess
		{
			texture2d_desc_t render_target_desc{};
			render_target_desc.clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			render_target_desc.clear_value.Color[0] = 0.0f;
			render_target_desc.clear_value.Color[1] = 0.0f;
			render_target_desc.clear_value.Color[2] = 0.0f;
			render_target_desc.clear_value.Color[3] = 0.0f;
			render_target_desc.width = width;
			render_target_desc.height = height;
			render_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			render_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			render_target_desc.start_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

			postprocess_textures[0] = Texture2D(gfx->GetDevice(), render_target_desc);
			postprocess_textures[0].CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			postprocess_textures[0].CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
			postprocess_textures[0].CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));

			render_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postprocess_textures[1] = Texture2D(gfx->GetDevice(), render_target_desc);
			postprocess_textures[1].CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			postprocess_textures[1].CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
			postprocess_textures[1].CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
		}

		//tiled deferred
		{

			texture2d_desc_t uav_target_desc{};
			uav_target_desc.width = width;
			uav_target_desc.height = height;
			uav_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			uav_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			uav_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			uav_target = Texture2D(gfx->GetDevice(), uav_target_desc);
			uav_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			uav_target.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));

			texture2d_desc_t tiled_debug_desc{};
			tiled_debug_desc.width = width;
			tiled_debug_desc.height = height;
			tiled_debug_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tiled_debug_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			tiled_debug_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			debug_tiled_texture = Texture2D(gfx->GetDevice(), tiled_debug_desc);
			debug_tiled_texture.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			debug_tiled_texture.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
		}

		//offscreen backbuffer
		{
			texture2d_desc_t offscreen_ldr_target_desc{};
			offscreen_ldr_target_desc.clear_value.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			offscreen_ldr_target_desc.clear_value.Color[0] = 0.0f;
			offscreen_ldr_target_desc.clear_value.Color[1] = 0.0f;
			offscreen_ldr_target_desc.clear_value.Color[2] = 0.0f;
			offscreen_ldr_target_desc.clear_value.Color[3] = 0.0f;
			offscreen_ldr_target_desc.width = width;
			offscreen_ldr_target_desc.height = height;
			offscreen_ldr_target_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
			offscreen_ldr_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			offscreen_ldr_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			offscreen_ldr_target = Texture2D(gfx->GetDevice(), offscreen_ldr_target_desc);

			offscreen_ldr_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			offscreen_ldr_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
		}

		//bokeh
		{
			bokeh = std::make_unique<StructuredBuffer<Bokeh>>(gfx->GetDevice(), width * height, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			bokeh->CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			bokeh->CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
			bokeh->CreateCounterUAV(uav_heap->GetCpuHandle(uav_heap_index++));
		}
		
		//velocity buffer
		{
			texture2d_desc_t velocity_buffer_desc{};
			velocity_buffer_desc.width = width;
			velocity_buffer_desc.height = height;
			velocity_buffer_desc.clear_value.Color[0] = 0.0f;
			velocity_buffer_desc.clear_value.Color[1] = 0.0f;
			velocity_buffer_desc.clear_value.Color[2] = 0.0f;
			velocity_buffer_desc.clear_value.Color[3] = 0.0f;
			velocity_buffer_desc.clear_value.Format = DXGI_FORMAT_R16G16_FLOAT;
			velocity_buffer_desc.format = DXGI_FORMAT_R16G16_FLOAT;
			velocity_buffer_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			velocity_buffer_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			velocity_buffer = Texture2D(gfx->GetDevice(), velocity_buffer_desc);
			velocity_buffer.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			velocity_buffer.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
		}

	}
	void Renderer::CreateOtherResources()
	{
		uint32 srv_heap_index = 0;
		uint32 dsv_heap_index = 0;
		uint32 uav_heap_index = 0;

		//shadow maps
		{
			//shadow map
			{
				texture2d_desc_t depth_map_desc{};
				depth_map_desc.width = SHADOW_MAP_SIZE;
				depth_map_desc.height = SHADOW_MAP_SIZE;
				depth_map_desc.format = DXGI_FORMAT_R32_TYPELESS;
				depth_map_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				depth_map_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				depth_map_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				depth_map_desc.clear_value.DepthStencil = { 1.0f, 0 };
				shadow_depth_map = Texture2D(gfx->GetDevice(), depth_map_desc);

				texture2d_srv_desc_t srv_desc{};
				srv_desc.format = DXGI_FORMAT_R32_FLOAT;
				shadow_depth_map.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++), &srv_desc);

				texture2d_dsv_desc_t dsv_desc{};
				dsv_desc.format = DXGI_FORMAT_D32_FLOAT;
				shadow_depth_map.CreateDSV(constant_dsv_heap->GetCpuHandle(dsv_heap_index++), &dsv_desc);
			}

			//shadow cubemap
			{
				texturecube_desc_t depth_cubemap_desc{};
				depth_cubemap_desc.width = SHADOW_CUBE_SIZE;
				depth_cubemap_desc.height = SHADOW_CUBE_SIZE;
				depth_cubemap_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				depth_cubemap_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				depth_cubemap_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				depth_cubemap_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				depth_cubemap_desc.clear_value.DepthStencil = { 1.0f, 0 };

				shadow_depth_cubemap = TextureCube(gfx->GetDevice(), depth_cubemap_desc);

				texturecube_srv_desc_t cube_srv_desc{};
				cube_srv_desc.format = DXGI_FORMAT_R32_FLOAT;
				shadow_depth_cubemap.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++), &cube_srv_desc);

				texturecube_dsv_desc_t cube_dsv_desc{};
				cube_dsv_desc.format = DXGI_FORMAT_D32_FLOAT;

				std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6> dsv_handles{};
				for (auto& dsv_handle : dsv_handles) dsv_handle = constant_dsv_heap->GetCpuHandle(dsv_heap_index++);

				shadow_depth_cubemap.CreateDSVs(dsv_handles, &cube_dsv_desc);
			}

			//shadow cascades
			{
				texture2darray_desc_t depth_cascade_maps_desc{};
				depth_cascade_maps_desc.width = SHADOW_CASCADE_MAP_SIZE;
				depth_cascade_maps_desc.height = SHADOW_CASCADE_MAP_SIZE;
				depth_cascade_maps_desc.array_size = CASCADE_COUNT;
				depth_cascade_maps_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				depth_cascade_maps_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				depth_cascade_maps_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				depth_cascade_maps_desc.clear_value.DepthStencil = { 1.0f, 0 };
				shadow_depth_cascades = Texture2DArray(gfx->GetDevice(), depth_cascade_maps_desc);

				texture2darray_srv_desc_t array_srv_desc{};
				array_srv_desc.format = DXGI_FORMAT_R32_FLOAT;
				shadow_depth_cascades.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++), &array_srv_desc);

				texture2darray_dsv_desc_t array_dsv_desc{};
				array_dsv_desc.format = DXGI_FORMAT_D32_FLOAT;

				std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsv_handles(CASCADE_COUNT);
				for (auto& dsv_handle : dsv_handles) dsv_handle = constant_dsv_heap->GetCpuHandle(dsv_heap_index++);

				shadow_depth_cascades.CreateDSVs(dsv_handles, &array_dsv_desc);
			}
		}

		//ao
		{
			//noise texture
			texture2d_desc_t noise_desc{};
			noise_desc.width = SSAO_NOISE_DIM;
			noise_desc.height = SSAO_NOISE_DIM;
			noise_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			noise_desc.start_state = D3D12_RESOURCE_STATE_COPY_DEST;
			noise_desc.clear_value.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

			ssao_random_texture = Texture2D(gfx->GetDevice(), noise_desc);
			hbao_random_texture = Texture2D(gfx->GetDevice(), noise_desc);

			ssao_random_texture.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			hbao_random_texture.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));

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

		//ocean
		{
			texture2d_desc_t uav_desc{};
			uav_desc.width = RESOLUTION;
			uav_desc.height = RESOLUTION;
			uav_desc.format = DXGI_FORMAT_R32_FLOAT;
			uav_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			uav_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			ocean_initial_spectrum = Texture2D(gfx->GetDevice(), uav_desc);
			ocean_initial_spectrum.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			ocean_initial_spectrum.CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

			uav_desc.start_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			ping_pong_phase_textures[pong_phase] = Texture2D(gfx->GetDevice(), uav_desc);
			ping_pong_phase_textures[pong_phase].CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			ping_pong_phase_textures[pong_phase].CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));
			uav_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			ping_pong_phase_textures[!pong_phase] = Texture2D(gfx->GetDevice(), uav_desc);
			ping_pong_phase_textures[!pong_phase].CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			ping_pong_phase_textures[!pong_phase].CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

			uav_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;

			uav_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			ping_pong_spectrum_textures[pong_spectrum] = Texture2D(gfx->GetDevice(), uav_desc);
			ping_pong_spectrum_textures[pong_spectrum].CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			ping_pong_spectrum_textures[pong_spectrum].CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

			uav_desc.start_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			ping_pong_spectrum_textures[!pong_spectrum] = Texture2D(gfx->GetDevice(), uav_desc);
			ping_pong_spectrum_textures[!pong_spectrum].CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			ping_pong_spectrum_textures[!pong_spectrum].CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

			uav_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			ocean_normal_map = Texture2D(gfx->GetDevice(), uav_desc);
			ocean_normal_map.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			ocean_normal_map.CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

		}

		//clustered deferred
		{

			clusters.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			clusters.CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

			light_counter.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			light_counter.CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

			light_list.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			light_list.CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));

			light_grid.CreateSRV(constant_srv_heap->GetCpuHandle(srv_heap_index++));
			light_grid.CreateUAV(constant_uav_heap->GetCpuHandle(uav_heap_index++));
		}

		picker.CreateView(constant_uav_heap->GetCpuHandle(uav_heap_index++));
	}
	void Renderer::CreateRenderPasses(uint32 width, uint32 height)
	{
		static D3D12_CLEAR_VALUE black = { .Format = DXGI_FORMAT_UNKNOWN, .Color = {0.0f, 0.0f, 0.0f, 1.0f}, };

		//gbuffer render pass
		{
			render_pass_desc_t render_pass_desc{};

			rtv_attachment_desc_t gbuffer_normal_attachment{};
			gbuffer_normal_attachment.cpu_handle = gbuffer[0].RTV();
			gbuffer_normal_attachment.clear_value = black;
			gbuffer_normal_attachment.clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			gbuffer_normal_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			gbuffer_normal_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(gbuffer_normal_attachment);

			rtv_attachment_desc_t gbuffer_albedo_attachment{};
			gbuffer_albedo_attachment.cpu_handle = gbuffer[1].RTV();
			gbuffer_albedo_attachment.clear_value = black;
			gbuffer_normal_attachment.clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			gbuffer_albedo_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			gbuffer_albedo_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(gbuffer_albedo_attachment);

			rtv_attachment_desc_t gbuffer_emissive_attachment{};
			gbuffer_emissive_attachment.cpu_handle = gbuffer[2].RTV();
			gbuffer_emissive_attachment.clear_value = black;
			gbuffer_emissive_attachment.clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			gbuffer_emissive_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			gbuffer_emissive_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(gbuffer_emissive_attachment);

			dsv_attachment_desc_t dsv_attachment_desc{};
			dsv_attachment_desc.cpu_handle = depth_target.DSV();
			dsv_attachment_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
			dsv_attachment_desc.clear_value.DepthStencil.Depth = 1.0f;
			dsv_attachment_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			dsv_attachment_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.dsv_attachment = dsv_attachment_desc;

			render_pass_desc.width = width;
			render_pass_desc.height = height;
			gbuffer_render_pass = RenderPass(render_pass_desc);
		}

		{
			render_pass_desc_t render_pass_desc{};

			rtv_attachment_desc_t decal_albedo_attachment{};
			decal_albedo_attachment.cpu_handle = gbuffer[1].RTV();
			decal_albedo_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			decal_albedo_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(decal_albedo_attachment);

			rtv_attachment_desc_t decal_normal_attachment{};
			decal_normal_attachment.cpu_handle = gbuffer[0].RTV();
			decal_normal_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			decal_normal_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(decal_normal_attachment);

			render_pass_desc.width = width;
			render_pass_desc.height = height;
			decal_pass = RenderPass(render_pass_desc);
		}

		//ambient render pass 
		{
			render_pass_desc_t render_pass_desc{};
			rtv_attachment_desc_t rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target.RTV();
			rtv_attachment_desc.clear_value = black;
			rtv_attachment_desc.clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);

			render_pass_desc.width = width;
			render_pass_desc.height = height;

			ambient_render_pass = RenderPass(render_pass_desc);
		}

		//lighting render pass
		{
			render_pass_desc_t render_pass_desc{};
			rtv_attachment_desc_t rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target.RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);

			render_pass_desc.width = width;
			render_pass_desc.height = height;

			lighting_render_pass = RenderPass(render_pass_desc);
		}

		//forward render pass
		{
			render_pass_desc_t render_pass_desc{};
			rtv_attachment_desc_t rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target.RTV();
			rtv_attachment_desc.clear_value = black;
			rtv_attachment_desc.clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);

			dsv_attachment_desc_t dsv_attachment_desc{};
			dsv_attachment_desc.cpu_handle = depth_target.DSV();
			dsv_attachment_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
			dsv_attachment_desc.clear_value.DepthStencil.Depth = 1.0f;
			dsv_attachment_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			dsv_attachment_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.dsv_attachment = dsv_attachment_desc;

			render_pass_desc.width = width;
			render_pass_desc.height = height;

			forward_render_pass = RenderPass(render_pass_desc);
		}

		//fxaa render pass
		{
			render_pass_desc_t render_pass_desc{};
			rtv_attachment_desc_t rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = ldr_render_target.RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);
			render_pass_desc.width = width;
			render_pass_desc.height = height;
			fxaa_render_pass = RenderPass(render_pass_desc);
		}

		//shadow map pass
		{
			render_pass_desc_t render_pass_desc{};
			dsv_attachment_desc_t dsv_attachment_desc{};
			dsv_attachment_desc.cpu_handle = shadow_depth_map.DSV();
			dsv_attachment_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
			dsv_attachment_desc.clear_value.DepthStencil.Depth = 1.0f;
			dsv_attachment_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			dsv_attachment_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.dsv_attachment = dsv_attachment_desc;

			render_pass_desc.width = SHADOW_MAP_SIZE;
			render_pass_desc.height = SHADOW_MAP_SIZE;

			shadow_map_pass = RenderPass(render_pass_desc);
		}

		//shadow cubemap passes
		{
			for (uint32 i = 0; i < shadow_cubemap_passes.size(); ++i)
			{
				render_pass_desc_t render_pass_desc{};
				dsv_attachment_desc_t dsv_attachment_desc{};
				dsv_attachment_desc.cpu_handle = shadow_depth_cubemap.DSV(i);
				dsv_attachment_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				dsv_attachment_desc.clear_value.DepthStencil.Depth = 1.0f;
				dsv_attachment_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
				dsv_attachment_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
				render_pass_desc.dsv_attachment = dsv_attachment_desc;

				render_pass_desc.width = SHADOW_CUBE_SIZE;
				render_pass_desc.height = SHADOW_CUBE_SIZE;

				shadow_cubemap_passes[i] = RenderPass(render_pass_desc);
			}

		}

		//shadow cascades passes
		{
			shadow_cascades_passes.clear();
			
			for (uint32 i = 0; i < CASCADE_COUNT; ++i)
			{
				render_pass_desc_t render_pass_desc{};
				dsv_attachment_desc_t dsv_attachment_desc{};
				dsv_attachment_desc.cpu_handle = shadow_depth_cascades.DSV(i);
				dsv_attachment_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				dsv_attachment_desc.clear_value.DepthStencil.Depth = 1.0f;
				dsv_attachment_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
				dsv_attachment_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
				render_pass_desc.dsv_attachment = dsv_attachment_desc;

				render_pass_desc.width = SHADOW_CASCADE_MAP_SIZE;
				render_pass_desc.height = SHADOW_CASCADE_MAP_SIZE;

				shadow_cascades_passes.emplace_back(render_pass_desc);
			}
		}

		//ssao render pass
		{
			render_pass_desc_t render_pass_desc{};
			rtv_attachment_desc_t ao_attachment_desc{};
			ao_attachment_desc.cpu_handle = ao_texture.RTV();
			ao_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			ao_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(ao_attachment_desc);
			render_pass_desc.width = width;
			render_pass_desc.height = height;
			
			ssao_render_pass = RenderPass(render_pass_desc);
			hbao_render_pass = RenderPass(render_pass_desc);
		}

		//particle pass
		{
			render_pass_desc_t render_pass_desc{};
			rtv_attachment_desc_t rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target.RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);

			render_pass_desc.width = width;
			render_pass_desc.height = height;
			particle_pass = RenderPass(render_pass_desc);
		}

		//postprocess passes
		{
			render_pass_desc_t ping_render_pass_desc{};
			rtv_attachment_desc_t ping_attachment_desc{};
			ping_attachment_desc.cpu_handle = postprocess_textures[0].RTV();
			ping_attachment_desc.clear_value = black;
			ping_attachment_desc.clear_value.Format = postprocess_textures[0].Format();
			ping_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			ping_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			ping_render_pass_desc.rtv_attachments.push_back(ping_attachment_desc);

			ping_render_pass_desc.width = width;
			ping_render_pass_desc.height = height;

			postprocess_passes[0] = RenderPass(ping_render_pass_desc);

			
			render_pass_desc_t pong_render_pass_desc{};
			rtv_attachment_desc_t pong_attachment_desc{};
			pong_attachment_desc.cpu_handle = postprocess_textures[1].RTV();
			pong_attachment_desc.clear_value = black;
			pong_attachment_desc.clear_value.Format = postprocess_textures[1].Format();
			pong_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			pong_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			pong_render_pass_desc.rtv_attachments.push_back(pong_attachment_desc);

			pong_render_pass_desc.width = width;
			pong_render_pass_desc.height = height;

			postprocess_passes[1] = RenderPass(pong_render_pass_desc);

		}

		//offscreen resolve pass
		{
			render_pass_desc_t render_pass_desc{};
			rtv_attachment_desc_t rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = offscreen_ldr_target.RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);
			render_pass_desc.width = width;
			render_pass_desc.height = height;
			offscreen_resolve_pass = RenderPass(render_pass_desc);
		}

		//velocity buffer pass
		{
			render_pass_desc_t render_pass_desc{};

			rtv_attachment_desc_t velocity_buffer_attachment{};
			velocity_buffer_attachment.cpu_handle = velocity_buffer.RTV();
			velocity_buffer_attachment.clear_value = black;
			velocity_buffer_attachment.clear_value.Format = DXGI_FORMAT_R16G16_FLOAT;
			velocity_buffer_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			velocity_buffer_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(velocity_buffer_attachment);

			render_pass_desc.width = width;
			render_pass_desc.height = height;
			velocity_buffer_pass = RenderPass(render_pass_desc);
		}
	}
	void Renderer::CreateIBLTextures()
	{
		ID3D12Device* device = gfx->GetDevice();
		auto cmd_list = gfx->GetDefaultCommandList();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		
		auto skyboxes = reg.view<Skybox>();
		TEXTURE_HANDLE unfiltered_env = INVALID_TEXTURE_HANDLE;
		for (auto skybox : skyboxes)
		{
			auto const& _skybox = skyboxes.get(skybox);
			if (_skybox.active)
			{
				unfiltered_env = _skybox.cubemap_texture;
				break;
			}
		}

		//create descriptor heap for ibl textures
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heap_desc.NumDescriptors = 3;
		ibl_heap = std::make_unique<DescriptorHeap>(device, heap_desc);

		ComPtr<ID3D12RootSignature> root_signature;
		// Create universal compute root signature.
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
			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			HRESULT hr = D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
			if (error) OutputDebugStringA((char*)error->GetBufferPointer());
			BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
		}

		ID3D12Resource* unfiltered_env_resource = texture_manager.Resource(unfiltered_env);
		ADRIA_ASSERT(unfiltered_env_resource);
		D3D12_RESOURCE_DESC unfiltered_env_desc = unfiltered_env_resource->GetDesc();
		
		//create env texture
		D3D12_RESOURCE_DESC env_desc = {};
		env_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		env_desc.Width = unfiltered_env_desc.Width;
		env_desc.Height = unfiltered_env_desc.Height;
		env_desc.DepthOrArraySize = 6;
		env_desc.MipLevels = 0;
		env_desc.Format = unfiltered_env_desc.Format;
		env_desc.SampleDesc.Count = 1;
		env_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
		BREAK_IF_FAILED(device->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&env_desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&env_texture)));
		
		env_desc = env_texture->GetDesc();


		D3D12_SHADER_RESOURCE_VIEW_DESC env_srv_desc{};
		env_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		env_srv_desc.Format = env_desc.Format;
		env_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		env_srv_desc.TextureCube.MipLevels = env_desc.MipLevels;
		env_srv_desc.TextureCube.MostDetailedMip = 0;

		device->CreateShaderResourceView(env_texture.Get(), &env_srv_desc, ibl_heap->GetCpuHandle(ENV_TEXTURE_SLOT));

		// Compute pre-filtered specular environment map.
		{
			ComPtr<ID3D12PipelineState> pipeline_state;

			ShaderBlob spmap_shader;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SpmapCS.cso", spmap_shader);

			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = root_signature.Get();
			pso_desc.CS = spmap_shader;
			BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));

			ResourceBarrierBatch precopy_barriers{};
			precopy_barriers.AddTransition(env_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			precopy_barriers.AddTransition(unfiltered_env_resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			precopy_barriers.Submit(cmd_list);

			
			for (uint32 array_slice = 0; array_slice < 6; ++array_slice)
			{
				const uint32 subresource_index = D3D12CalcSubresource(0, array_slice, 0, env_desc.MipLevels, 6);
				const uint32 unfiltered_subresource_index = D3D12CalcSubresource(0, array_slice, 0, unfiltered_env_desc.MipLevels, 6);
				auto dst_copy_region = CD3DX12_TEXTURE_COPY_LOCATION{ env_texture.Get(), subresource_index };
				auto src_copy_region = CD3DX12_TEXTURE_COPY_LOCATION{ unfiltered_env_resource, unfiltered_subresource_index };
				cmd_list->CopyTextureRegion(&dst_copy_region, 0, 0, 0, &src_copy_region, nullptr);
			}
			
			ResourceBarrierBatch postcopy_barriers{};
			postcopy_barriers.AddTransition(env_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			postcopy_barriers.AddTransition(unfiltered_env_resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			postcopy_barriers.Submit(cmd_list);

			cmd_list->SetPipelineState(pipeline_state.Get());
			cmd_list->SetComputeRootSignature(root_signature.Get());

			auto unfiltered_env_descriptor = texture_manager.CpuDescriptorHandle(unfiltered_env);
			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), unfiltered_env_descriptor,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));

			float delta_roughness = 1.0f / std::max<float32>(float32(env_desc.MipLevels - 1), 1.0f);
			for (uint32 level = 1, size = (uint32)std::max<uint64>(unfiltered_env_desc.Width, unfiltered_env_desc.Height) / 2; level < env_desc.MipLevels; ++level, size /= 2)
			{
				const uint32 num_groups = std::max<uint32>(1, size / 32);
				const float spmap_roughness = level * delta_roughness;

				D3D12_RESOURCE_DESC desc = env_texture->GetDesc();
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = desc.Format;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.MipSlice = level;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;

				OffsetType descriptor_index = descriptor_allocator->Allocate();

				device->CreateUnorderedAccessView(env_texture.Get(), nullptr,
					&uavDesc,
					descriptor_allocator->GetCpuHandle(descriptor_index));

				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
				cmd_list->SetComputeRoot32BitConstants(2, 1, &spmap_roughness, 0);
				cmd_list->Dispatch(num_groups, num_groups, 6);
			}

			auto env_barrier = CD3DX12_RESOURCE_BARRIER::Transition(env_texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			cmd_list->ResourceBarrier(1, &env_barrier);

			gfx->ExecuteDefaultCommandList();
			gfx->WaitForGPU();
			gfx->ResetDefaultCommandList();
		}

		//create irmap texture
		D3D12_RESOURCE_DESC irmap_desc = {};
		irmap_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		irmap_desc.Width = 32;
		irmap_desc.Height = 32;
		irmap_desc.DepthOrArraySize = 6;
		irmap_desc.MipLevels = 1;
		irmap_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		irmap_desc.SampleDesc.Count = 1;
		irmap_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
		BREAK_IF_FAILED(device->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&irmap_desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&irmap_texture)));

		D3D12_SHADER_RESOURCE_VIEW_DESC irmap_srv_desc{};
		irmap_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		irmap_srv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		irmap_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		irmap_srv_desc.TextureCube.MipLevels = 1;
		irmap_srv_desc.TextureCube.MostDetailedMip = 0;

		device->CreateShaderResourceView(irmap_texture.Get(), &irmap_srv_desc, ibl_heap->GetCpuHandle(IRMAP_TEXTURE_SLOT));


		// Compute diffuse irradiance cubemap.
		{
			ComPtr<ID3D12PipelineState> pipeline_state;
			ShaderBlob irmap_shader;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/IrmapCS.cso", irmap_shader);

			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = root_signature.Get();
			pso_desc.CS = irmap_shader;
			BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));


			D3D12_RESOURCE_DESC desc = irmap_texture->GetDesc();
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = desc.Format;
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav_desc.Texture2DArray.MipSlice = 0;
			uav_desc.Texture2DArray.FirstArraySlice = 0;
			uav_desc.Texture2DArray.ArraySize = desc.DepthOrArraySize;

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

			device->CreateUnorderedAccessView(irmap_texture.Get(), nullptr,
				&uav_desc,
				descriptor_allocator->GetCpuHandle(descriptor_index));

			auto irmap_barrier = CD3DX12_RESOURCE_BARRIER::Transition(irmap_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd_list->ResourceBarrier(1, &irmap_barrier);
			cmd_list->SetPipelineState(pipeline_state.Get());
			cmd_list->SetComputeRootSignature(root_signature.Get());
			ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
			cmd_list->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);

			device->CopyDescriptorsSimple(1,
				descriptor_allocator->GetCpuHandle(descriptor_index + 1), ibl_heap->GetCpuHandle(ENV_TEXTURE_SLOT),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index + 1));
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch((uint32)desc.Width / 32, (uint32)desc.Height / 32, 6u);
			irmap_barrier = CD3DX12_RESOURCE_BARRIER::Transition(irmap_texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			cmd_list->ResourceBarrier(1, &irmap_barrier);

			gfx->ExecuteDefaultCommandList();
			gfx->WaitForGPU();
			gfx->ResetDefaultCommandList();
		}


		// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.

		D3D12_RESOURCE_DESC brdf_desc = {};
		brdf_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		brdf_desc.Width = 256;
		brdf_desc.Height = 256;
		brdf_desc.DepthOrArraySize = 1;
		brdf_desc.MipLevels = 1;
		brdf_desc.Format = DXGI_FORMAT_R16G16_FLOAT;
		brdf_desc.SampleDesc.Count = 1;
		brdf_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
		BREAK_IF_FAILED(device->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&brdf_desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&brdf_lut_texture)));

		D3D12_SHADER_RESOURCE_VIEW_DESC brdf_srv_desc{};
		brdf_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		brdf_srv_desc.Format = DXGI_FORMAT_R16G16_FLOAT;
		brdf_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		brdf_srv_desc.TextureCube.MipLevels = 1;
		brdf_srv_desc.TextureCube.MostDetailedMip = 0;

		device->CreateShaderResourceView(brdf_lut_texture.Get(), &brdf_srv_desc, ibl_heap->GetCpuHandle(BRDF_LUT_TEXTURE_SLOT));

		{
			auto desc = brdf_lut_texture->GetDesc();

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
			uav_desc.Format = desc.Format;
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uav_desc.Texture2DArray.MipSlice = 0;
			uav_desc.Texture2DArray.FirstArraySlice = 0;
			uav_desc.Texture2DArray.ArraySize = desc.DepthOrArraySize;

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CreateUnorderedAccessView(brdf_lut_texture.Get(), nullptr,
				&uav_desc,
				descriptor_allocator->GetCpuHandle(descriptor_index));

			ComPtr<ID3D12PipelineState> pipeline_state;
			ShaderBlob BRDFShader;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SpbrdfCS.cso", BRDFShader);

			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = root_signature.Get();
			pso_desc.CS = BRDFShader;
			BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));

			auto brdf_barrier = CD3DX12_RESOURCE_BARRIER::Transition(brdf_lut_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd_list->ResourceBarrier(1, &brdf_barrier);
			cmd_list->SetPipelineState(pipeline_state.Get());
			cmd_list->SetComputeRootSignature(root_signature.Get());
			ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
			cmd_list->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);

			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch((uint32)desc.Width / 32, (uint32)desc.Height / 32, 1);

			brdf_barrier = CD3DX12_RESOURCE_BARRIER::Transition(brdf_lut_texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			cmd_list->ResourceBarrier(1, &brdf_barrier);

			gfx->ExecuteDefaultCommandList();
			gfx->WaitForGPU();
			gfx->ResetDefaultCommandList();

			cmd_list->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);
		}

		ibl_textures_generated = true;
	}

	void Renderer::UpdateConstantBuffers(float32 dt)
	{
		//frame
		{
			frame_cbuf_data.global_ambient = XMVectorSet(settings.ambient_color[0], settings.ambient_color[1], settings.ambient_color[2], 1);
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
			frame_cbuf_data.mouse_normalized_coords_x = (current_scene_viewport.mouse_position_x - current_scene_viewport.scene_viewport_pos_x) / current_scene_viewport.scene_viewport_size_x;
			frame_cbuf_data.mouse_normalized_coords_y = (current_scene_viewport.mouse_position_y - current_scene_viewport.scene_viewport_pos_y) / current_scene_viewport.scene_viewport_size_y;

			frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);

			frame_cbuf_data.prev_view_projection = camera->ViewProj();
		}

		//postprocess
		{
			PostprocessCBuffer postprocess_cbuf_data{};
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

		//compute 
		{
			std::array<float32, 9> coeffs = GaussKernel<4>(settings.blur_sigma);
			//coeffs.fill(1.0f / 9);

			ComputeCBuffer compute_cbuf_data{};
			compute_cbuf_data.gauss_coeff1 = coeffs[0];
			compute_cbuf_data.gauss_coeff2 = coeffs[1];
			compute_cbuf_data.gauss_coeff3 = coeffs[2];
			compute_cbuf_data.gauss_coeff4 = coeffs[3];
			compute_cbuf_data.gauss_coeff5 = coeffs[4];
			compute_cbuf_data.gauss_coeff6 = coeffs[5];
			compute_cbuf_data.gauss_coeff7 = coeffs[6];
			compute_cbuf_data.gauss_coeff8 = coeffs[7];
			compute_cbuf_data.gauss_coeff9 = coeffs[8];
			compute_cbuf_data.bloom_scale = settings.bloom_scale;
			compute_cbuf_data.threshold = settings.bloom_threshold;
			compute_cbuf_data.visualize_tiled = settings.visualize_tiled;
			compute_cbuf_data.visualize_max_lights = settings.visualize_max_lights;
			compute_cbuf_data.bokeh_blur_threshold = settings.bokeh_blur_threshold;
			compute_cbuf_data.bokeh_lum_threshold = settings.bokeh_lum_threshold;
			compute_cbuf_data.dof_params = XMVectorSet(settings.dof_near_blur, settings.dof_near, settings.dof_far, settings.dof_far_blur);
			compute_cbuf_data.bokeh_radius_scale = settings.bokeh_radius_scale;
			compute_cbuf_data.bokeh_color_scale = settings.bokeh_color_scale;
			compute_cbuf_data.bokeh_fallout = settings.bokeh_fallout;

			compute_cbuf_data.ocean_choppiness = settings.ocean_choppiness;
			compute_cbuf_data.ocean_size = 512;
			compute_cbuf_data.resolution = RESOLUTION;
			compute_cbuf_data.wind_direction_x = settings.wind_direction[0];
			compute_cbuf_data.wind_direction_y = settings.wind_direction[1];
			compute_cbuf_data.delta_time = dt;

			compute_cbuffer.Update(compute_cbuf_data, backbuffer_index);
		}

		//weather
		{
			WeatherCBuffer weather_cbuf_data{};
			static float32 total_time = 0.0f;
			total_time += dt;

			auto lights = reg.view<Light>();

			for (auto light : lights)
			{
				auto const& light_data = lights.get(light);

				if (light_data.type == ELightType::Directional && light_data.active)
				{
					weather_cbuf_data.light_dir = XMVector3Normalize(-light_data.direction);
					weather_cbuf_data.light_color = light_data.color * light_data.energy;
					break;
				}
			}

			weather_cbuf_data.sky_color = XMVECTOR{ settings.sky_color[0], settings.sky_color[1],settings.sky_color[2], 1.0f };
			weather_cbuf_data.ambient_color = XMVECTOR{ settings.ambient_color[0], settings.ambient_color[1], settings.ambient_color[2], 1.0f };
			weather_cbuf_data.wind_dir = XMVECTOR{ settings.wind_direction[0], 0.0f, settings.wind_direction[1], 0.0f };
			weather_cbuf_data.wind_speed = settings.wind_speed;
			weather_cbuf_data.time = total_time;
			weather_cbuf_data.crispiness = settings.crispiness;
			weather_cbuf_data.curliness = settings.curliness;
			weather_cbuf_data.coverage = settings.coverage;
			weather_cbuf_data.absorption = settings.light_absorption;
			weather_cbuf_data.clouds_bottom_height = settings.clouds_bottom_height;
			weather_cbuf_data.clouds_top_height = settings.clouds_top_height;
			weather_cbuf_data.density_factor = settings.density_factor;
			weather_cbuf_data.cloud_type = settings.cloud_type;

			XMFLOAT3 sun_dir;
			XMStoreFloat3(&sun_dir, XMVector3Normalize(weather_cbuf_data.light_dir));
			SkyParameters sky_params = CalculateSkyParameters(settings.turbidity, settings.ground_albedo, sun_dir);

			weather_cbuf_data.A = sky_params[(size_t)ESkyParams::A];
			weather_cbuf_data.B = sky_params[(size_t)ESkyParams::B];
			weather_cbuf_data.C = sky_params[(size_t)ESkyParams::C];
			weather_cbuf_data.D = sky_params[(size_t)ESkyParams::D];
			weather_cbuf_data.E = sky_params[(size_t)ESkyParams::E];
			weather_cbuf_data.F = sky_params[(size_t)ESkyParams::F];
			weather_cbuf_data.G = sky_params[(size_t)ESkyParams::G];
			weather_cbuf_data.H = sky_params[(size_t)ESkyParams::H];
			weather_cbuf_data.I = sky_params[(size_t)ESkyParams::I];
			weather_cbuf_data.Z = sky_params[(size_t)ESkyParams::Z];

			weather_cbuffer.Update(weather_cbuf_data, backbuffer_index);
		}

		particle_renderer.SetCBuffersForThisFrame(frame_cbuffer.View(backbuffer_index).BufferLocation,
			compute_cbuffer.View(backbuffer_index).BufferLocation);
	}
	void Renderer::UpdateParticles(float32 dt)
	{
		auto emitters = reg.view<Emitter>();
		for (auto emitter : emitters)
		{
			Emitter& emitter_params = emitters.get(emitter);
			particle_renderer.Update(dt, emitter_params);
		}
	}
	void Renderer::CameraFrustumCulling()
	{
		BoundingFrustum camera_frustum = camera->Frustum();

		auto visibility_view = reg.view<Visibility>();

		for (auto e : visibility_view)
		{
			auto& visibility = visibility_view.get(e);

			visibility.camera_visible = camera_frustum.Intersects(visibility.aabb) || reg.has<Light>(e); //dont cull lights for now
		}
	} 
	void Renderer::LightFrustumCulling(ELightType type)
	{
		BoundingFrustum camera_frustum = camera->Frustum();

		auto visibility_view = reg.view<Visibility>();

		for (auto e : visibility_view)
		{
			auto& visibility = visibility_view.get(e);

			switch (type)
			{
			case ELightType::Directional:
				visibility.light_visible = light_bounding_box.Intersects(visibility.aabb);
				break;
			case ELightType::Spot:
			case ELightType::Point:
				visibility.light_visible = light_bounding_frustum.Intersects(visibility.aabb);
				break;
			default:
				ADRIA_ASSERT(false);
			}
		}
	}

	void Renderer::PassPicking(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Picking Pass");
		picker.Pick(cmd_list, depth_target.SRV(), gbuffer[0].SRV(), frame_cbuffer.View(backbuffer_index).BufferLocation);
	}
	void Renderer::PassGBuffer(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "GBuffer Pass");
		SCOPED_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, EProfilerBlock::GBufferPass, profiler_settings.profile_gbuffer_pass);

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();
		auto gbuffer_view = reg.view<Mesh, Transform, Material, Deferred, Visibility>();

		ResourceBarrierBatch gbuffer_barriers{};
		for (auto& texture : gbuffer)
			texture.Transition(gbuffer_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		gbuffer_barriers.Submit(cmd_list);

		gbuffer_render_pass.Begin(cmd_list);

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::GbufferPBR].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::GbufferPBR].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

		for (auto e : gbuffer_view)
		{
			auto [mesh, transform, material, visibility] = gbuffer_view.get<Mesh, Transform, Material, Visibility>(e);

			if (!visibility.camera_visible) continue;

			object_cbuf_data.model = transform.current_transform;
			object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, object_cbuf_data.model));

			object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			object_allocation.Update(object_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

			material_cbuf_data.albedo_factor = material.albedo_factor;
			material_cbuf_data.metallic_factor = material.metallic_factor;
			material_cbuf_data.roughness_factor = material.roughness_factor;
			material_cbuf_data.emissive_factor = material.emissive_factor;

			DynamicAllocation material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			material_allocation.Update(material_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles{};
			std::vector<uint32> src_range_sizes{};

			ADRIA_ASSERT(material.albedo_texture != INVALID_TEXTURE_HANDLE);

			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.albedo_texture));
			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.metallic_roughness_texture));
			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.normal_texture));
			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.emissive_texture));
			src_range_sizes.assign(texture_handles.size(), 1u);

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(texture_handles.size());
			auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)texture_handles.size() };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, (uint32)texture_handles.size(), texture_handles.data(), src_range_sizes.data(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

			mesh.Draw(cmd_list);
		}
		gbuffer_render_pass.End(cmd_list);

		if (reg.size<Decal>() > 0)
		{
			gbuffer_barriers.Clear();
			gbuffer_barriers.AddTransition(gbuffer[2].Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			gbuffer_barriers.Submit(cmd_list);
		}
		else
		{
			gbuffer_barriers.ReverseTransitions();
			gbuffer_barriers.Submit(cmd_list);
		}


	}
	void Renderer::PassDecals(ID3D12GraphicsCommandList4* cmd_list)
	{
		if (reg.size<Decal>() == 0) return;
		SCOPED_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, EProfilerBlock::DecalPass, profiler_settings.profile_decal_pass);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Decal Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		struct DecalCBuffer
		{
			int32 decal_type;
		};
		DecalCBuffer decal_cbuf_data{};
		ObjectCBuffer object_cbuf_data{};

		decal_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Decals].Get());
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			auto decal_view = reg.view<Decal>();

			auto decal_pass_lambda = [&](bool modify_normals)
			{
				modify_normals ? cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Decals_ModifyNormals].Get()) : cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Decals].Get());
				for (auto e : decal_view)
				{
					Decal decal = decal_view.get(e);
					if (decal.modify_gbuffer_normals != modify_normals) continue;

					object_cbuf_data.model = decal.decal_model_matrix;
					object_cbuf_data.inverse_transposed_model = XMMatrixTranspose(XMMatrixInverse(nullptr, object_cbuf_data.model));
					object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					object_allocation.Update(object_cbuf_data);
					cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
					cmd_list->SetGraphicsRoot32BitConstant(2, static_cast<UINT>(decal.decal_type), 0);

					std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles{};
					std::vector<uint32> src_range_sizes{};

					texture_handles.push_back(texture_manager.CpuDescriptorHandle(decal.albedo_decal_texture));
					texture_handles.push_back(texture_manager.CpuDescriptorHandle(decal.normal_decal_texture));
					texture_handles.push_back(depth_target.SRV());
					src_range_sizes.assign(texture_handles.size(), 1u);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(texture_handles.size());
					auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)texture_handles.size() };
					device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, (uint32)texture_handles.size(), texture_handles.data(), src_range_sizes.data(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

					cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					cube_vb->Bind(cmd_list);
					cube_ib->Bind(cmd_list);
					cmd_list->DrawIndexedInstanced(cube_ib->IndexCount(), 1, 0, 0, 0);
				}
			};
			decal_pass_lambda(false);
			decal_pass_lambda(true);
		}
		decal_pass.End(cmd_list);

		ResourceBarrierBatch decal_barriers{};
		decal_barriers.AddTransition(gbuffer[0].Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		decal_barriers.AddTransition(gbuffer[1].Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		decal_barriers.Submit(cmd_list);
	}

	void Renderer::PassSSAO(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.ambient_occlusion == EAmbientOcclusion::SSAO);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "SSAO Pass");

		ResourceBarrierBatch ssao_barrier{};
		ao_texture.Transition(ssao_barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		ssao_barrier.Submit(cmd_list);

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		ssao_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::AO].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::SSAO].Get());

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
			D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0].SRV(), depth_target.SRV(), ssao_random_texture.SRV() };
			D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
			UINT src_range_sizes[] = { 1, 1, 1 };
			UINT dst_range_sizes[] = { 3 };
			device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmd_list->DrawInstanced(4, 1, 0, 0);
		}
		ssao_render_pass.End(cmd_list);


		ssao_barrier.ReverseTransitions();
		ssao_barrier.Submit(cmd_list);

		BlurTexture(cmd_list, ao_texture);
	}
	void Renderer::PassHBAO(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.ambient_occlusion == EAmbientOcclusion::HBAO);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "HBAO Pass");

		ResourceBarrierBatch hbao_barrier{};
		ao_texture.Transition(hbao_barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		hbao_barrier.Submit(cmd_list);

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		hbao_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::AO].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::HBAO].Get());

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
			D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0].SRV(), depth_target.SRV(), hbao_random_texture.SRV() };
			D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
			UINT src_range_sizes[] = { 1, 1, 1 };
			UINT dst_range_sizes[] = { 3 };
			device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmd_list->DrawInstanced(4, 1, 0, 0);
		}
		hbao_render_pass.End(cmd_list);

		hbao_barrier.ReverseTransitions();
		hbao_barrier.Submit(cmd_list);

		BlurTexture(cmd_list, ao_texture);
	}
	void Renderer::PassAmbient(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Ambient Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		if (!ibl_textures_generated) settings.ibl = false;

		ambient_render_pass.Begin(cmd_list);

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::AmbientPBR].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

		bool has_ao = settings.ambient_occlusion != EAmbientOcclusion::None;
		if (has_ao && settings.ibl) cmd_list->SetPipelineState(pso_map[EPipelineStateObject::AmbientPBR_AO_IBL].Get());
		else if (has_ao && !settings.ibl) cmd_list->SetPipelineState(pso_map[EPipelineStateObject::AmbientPBR_AO].Get());
		else if (!has_ao && settings.ibl) cmd_list->SetPipelineState(pso_map[EPipelineStateObject::AmbientPBR_IBL].Get());
		else cmd_list->SetPipelineState(pso_map[EPipelineStateObject::AmbientPBR].Get());

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = {gbuffer[0].SRV(), gbuffer[1].SRV(), gbuffer[2].SRV(), depth_target.SRV()};
		uint32 src_range_sizes[] = {1,1,1,1};
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
		auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
		device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));


		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { null_srv_heap->GetCpuHandle(TEXTURE2D_SLOT), 
		null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT), null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT), null_srv_heap->GetCpuHandle(TEXTURE2D_SLOT) };
		uint32 src_range_sizes2[] = { 1,1,1,1 };
		if (has_ao) cpu_handles2[0] = blur_final_texture.SRV(); //contains blurred ssao
		if (settings.ibl)
		{
			cpu_handles2[1] = ibl_heap->GetCpuHandle(ENV_TEXTURE_SLOT);
			cpu_handles2[2] = ibl_heap->GetCpuHandle(IRMAP_TEXTURE_SLOT);
			cpu_handles2[3] = ibl_heap->GetCpuHandle(BRDF_LUT_TEXTURE_SLOT);
		}

		descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles2));
		dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		uint32 dst_range_sizes2[] = { (uint32)ARRAYSIZE(cpu_handles2) };
		device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes2, ARRAYSIZE(cpu_handles2), cpu_handles2, src_range_sizes2,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

		ambient_render_pass.End(cmd_list);
	}
	void Renderer::PassDeferredLighting(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Deferred Lighting Pass");
		SCOPED_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, EProfilerBlock::DeferredPass, profiler_settings.profile_deferred_pass);

		ID3D12Device* device = gfx->GetDevice();
		auto upload_buffer = gfx->GetUploadBuffer();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		auto lights = reg.view<Light>();
		for (entity light : lights)
		{
			auto const& light_data = lights.get(light);
			if (!light_data.active) continue;
			if ((settings.use_tiled_deferred || settings.use_clustered_deferred) && !light_data.casts_shadows) continue; //tiled deferred takes care of noncasting lights

			//update cbuffer

			light_cbuf_data.active = light_data.active;
			light_cbuf_data.casts_shadows = light_data.casts_shadows;
			light_cbuf_data.color = light_data.color * light_data.energy;
			light_cbuf_data.direction = light_data.direction;
			light_cbuf_data.inner_cosine = light_data.inner_cosine;
			light_cbuf_data.outer_cosine = light_data.outer_cosine;
			light_cbuf_data.position = light_data.position;
			light_cbuf_data.range = light_data.range;
			light_cbuf_data.type = static_cast<int32>(light_data.type);
			light_cbuf_data.use_cascades = light_data.use_cascades;
			light_cbuf_data.volumetric = light_data.volumetric;
			light_cbuf_data.volumetric_strength = light_data.volumetric_strength;
			light_cbuf_data.sscs = light_data.sscs;
			light_cbuf_data.sscs_thickness = light_data.sscs_thickness;
			light_cbuf_data.sscs_max_ray_distance = light_data.sscs_max_ray_distance;
			light_cbuf_data.sscs_max_depth_distance = light_data.sscs_max_depth_distance;

			XMMATRIX camera_view = camera->View();
			light_cbuf_data.position = DirectX::XMVector4Transform(light_cbuf_data.position, camera_view);
			light_cbuf_data.direction = DirectX::XMVector4Transform(light_cbuf_data.direction, camera_view);

			light_allocation = upload_buffer->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			light_allocation.Update(light_cbuf_data);

			//shadow mapping
			
			if (light_data.casts_shadows)
			{
				switch (light_data.type)
				{
				case ELightType::Directional:
					if (light_data.use_cascades) PassShadowMapCascades(cmd_list, light_data);
					else PassShadowMapDirectional(cmd_list, light_data);
					break;
				case ELightType::Spot:
					PassShadowMapSpot(cmd_list, light_data);
					break;
				case ELightType::Point:
					PassShadowMapPoint(cmd_list, light_data);
					break;
				default:
					ADRIA_ASSERT(false);
				}
			}

			//lighting + volumetric fog
			lighting_render_pass.Begin(cmd_list);
			{
				cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::LightingPBR].Get());
				cmd_list->SetPipelineState(pso_map[EPipelineStateObject::LightingPBR].Get());

				cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

				cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);

				cmd_list->SetGraphicsRootConstantBufferView(2, shadow_allocation.gpu_address);

				//t0,t1,t2 - gbuffer and depth
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0].SRV(), gbuffer[1].SRV(), depth_target.SRV() };
					uint32 src_range_sizes[] = { 1,1,1 };

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
					D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
					device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));
				}

				//t4,t5,t6 - shadow maps
				{
					D3D12_CPU_DESCRIPTOR_HANDLE shadow_cpu_handles[] = { null_srv_heap->GetCpuHandle(TEXTURE2D_SLOT),
						null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT), null_srv_heap->GetCpuHandle(TEXTURE2DARRAY_SLOT) }; 
					uint32 src_range_sizes[] = { 1,1,1 };

					switch (light_data.type)
					{
					case ELightType::Directional:
						if (light_data.use_cascades) shadow_cpu_handles[TEXTURE2DARRAY_SLOT] = shadow_depth_cascades.SRV();
						else shadow_cpu_handles[TEXTURE2D_SLOT] = shadow_depth_map.SRV();
						break;
					case ELightType::Spot:
						shadow_cpu_handles[TEXTURE2D_SLOT] = shadow_depth_map.SRV();
						break;
					case ELightType::Point:
						shadow_cpu_handles[TEXTURECUBE_SLOT] = shadow_depth_cubemap.SRV();
						break;
					default:
						ADRIA_ASSERT(false);
					}

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(shadow_cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(shadow_cpu_handles) };
					device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(shadow_cpu_handles), shadow_cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetGraphicsRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));
				}

				cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				cmd_list->DrawInstanced(4, 1, 0, 0);
				
				if (light_data.volumetric) PassVolumetric(cmd_list, light_data);
			}
			lighting_render_pass.End(cmd_list);

		}
	}
	void Renderer::PassDeferredTiledLighting(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.use_tiled_deferred);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Deferred Tiled Lighting Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto upload_buffer = gfx->GetUploadBuffer();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		auto light_view = reg.view<Light>();
		std::vector<StructuredLight> structured_lights{};
		for (auto e : light_view)
		{
			StructuredLight structured_light{};
			auto& light = light_view.get(e);
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


		ResourceBarrierBatch tiled_barriers{};
		depth_target.Transition(tiled_barriers, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gbuffer[0].Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gbuffer[1].Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		uav_target.Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		debug_tiled_texture.Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		tiled_barriers.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(rs_map[ERootSignature::TiledLighting].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::TiledLighting].Get());

		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(1, compute_cbuffer.View(backbuffer_index).BufferLocation);

		//t0,t1,t2 - gbuffer and depth
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0].SRV(), gbuffer[1].SRV(), depth_target.SRV() };
			uint32 src_range_sizes[] = { 1,1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		}

		D3D12_GPU_DESCRIPTOR_HANDLE uav_target_for_clear{};
		D3D12_GPU_DESCRIPTOR_HANDLE uav_debug_for_clear{};
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { uav_target.UAV(), debug_tiled_texture.UAV() };
			uint32 src_range_sizes[] = { 1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

			uav_target_for_clear = descriptor_allocator->GetGpuHandle(descriptor_index);
			uav_debug_for_clear = descriptor_allocator->GetGpuHandle(descriptor_index + 1);
		}

		{
			uint64 alloc_size = sizeof(StructuredLight) * structured_lights.size();
			DynamicAllocation dynamic_alloc = upload_buffer->Allocate(alloc_size, sizeof(StructuredLight));
			dynamic_alloc.Update(structured_lights.data(), alloc_size);

			auto i = descriptor_allocator->Allocate();

			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
			desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;

			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, descriptor_allocator->GetCpuHandle(i));

			cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(i));
		}

		float32 black[4] = { 0.0f,0.0f,0.0f,0.0f };

		cmd_list->ClearUnorderedAccessViewFloat(uav_target_for_clear, uav_target.UAV(), uav_target.Resource(),
			black, 0, nullptr);
		cmd_list->ClearUnorderedAccessViewFloat(uav_debug_for_clear, debug_tiled_texture.UAV(), debug_tiled_texture.Resource(),
			black, 0, nullptr);

		cmd_list->Dispatch((uint32)std::ceil(width * 1.0f / 16), (uint32)(height * 1.0f / 16), 1);

		tiled_barriers.ReverseTransitions();
		tiled_barriers.Submit(cmd_list);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = hdr_render_target.RTV();
		cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		if (settings.visualize_tiled) AddTextures(cmd_list, uav_target, debug_tiled_texture, EBlendMode::AlphaBlend);
		else CopyTexture(cmd_list, uav_target, EBlendMode::AdditiveBlend);
	}
	void Renderer::PassDeferredClusteredLighting(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.use_clustered_deferred);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Deferred Clustered Lighting Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto upload_buffer = gfx->GetUploadBuffer();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		auto light_view = reg.view<Light>();
		std::vector<StructuredLight> structured_lights{};
		for (auto e : light_view)
		{
			StructuredLight structured_light{};
			auto& light = light_view.get(e);
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
		uint64 alloc_size = sizeof(StructuredLight) * structured_lights.size();
		DynamicAllocation dynamic_alloc = upload_buffer->Allocate(alloc_size, sizeof(StructuredLight));
		dynamic_alloc.Update(structured_lights.data(), alloc_size);

		//cluster building
		if (recreate_clusters)
		{
			cmd_list->SetComputeRootSignature(rs_map[ERootSignature::ClusterBuilding].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::ClusterBuilding].Get());
			cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			device->CopyDescriptorsSimple(1, dst_descriptor, clusters.UAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

			cmd_list->Dispatch(CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
			recreate_clusters = false;
		}

		//cluster building
		{
			cmd_list->SetComputeRootSignature(rs_map[ERootSignature::ClusterCulling].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::ClusterCulling].Get());
		
			OffsetType i = descriptor_allocator->AllocateRange(2);
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(i);
			device->CopyDescriptorsSimple(1, dst_descriptor, clusters.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
			desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;
			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, descriptor_allocator->GetCpuHandle(i + 1));
		
			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(i));
		
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { light_counter.UAV(), light_list.UAV(), light_grid.UAV() };
			uint32 src_range_sizes[] = { 1,1,1 };
			i = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			dst_descriptor = descriptor_allocator->GetCpuHandle(i);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(i));
			cmd_list->Dispatch(CLUSTER_SIZE_X / 16, CLUSTER_SIZE_Y / 16, CLUSTER_SIZE_Z / 1);
		}


		//clustered lighting
		lighting_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::ClusteredLightingPBR].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::ClusteredLightingPBR].Get());
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

			//gbuffer
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0].SRV(), gbuffer[1].SRV(), depth_target.SRV() };
			uint32 src_range_sizes[] = { 1,1,1 };
			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

			//light stuff
			descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles) + 1);
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { light_list.SRV(), light_grid.SRV() };
			uint32 src_range_sizes2[] = { 1,1 };

			dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index + 1);
			uint32 dst_range_sizes2[] = { (uint32)ARRAYSIZE(cpu_handles2) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes2, ARRAYSIZE(cpu_handles2), cpu_handles2, src_range_sizes2,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
			desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;
			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, descriptor_allocator->GetCpuHandle(descriptor_index));

			cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmd_list->DrawInstanced(4, 1, 0, 0);
		}
		lighting_render_pass.End(cmd_list);

	}
	void Renderer::PassForward(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Forward Pass");
		SCOPED_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, EProfilerBlock::ForwardPass, profiler_settings.profile_forward_pass);

		UpdateOcean(cmd_list);
		forward_render_pass.Begin(cmd_list);
		PassOcean(cmd_list);
		PassForwardCommon(cmd_list, false);
		PassSky(cmd_list);
		PassForwardCommon(cmd_list, true);
		forward_render_pass.End(cmd_list);
	}
	void Renderer::PassPostprocess(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Postprocessing Pass");
		SCOPED_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, EProfilerBlock::Postprocessing, profiler_settings.profile_postprocessing);

		PassVelocityBuffer(cmd_list);

		auto lights = reg.view<Light>();

		postprocess_passes[postprocess_index].Begin(cmd_list); //set ping as rt

		CopyTexture(cmd_list, hdr_render_target);

		for (entity light : lights)
		{
			auto const& light_data = lights.get(light);
			if (!light_data.active || !light_data.lens_flare) continue;

			PassLensFlare(cmd_list, light_data);
		}
		
		postprocess_passes[postprocess_index].End(cmd_list); //now we have copy of scene in ping

		ResourceBarrierBatch postprocess_barriers{};
		postprocess_barriers.AddTransition(postprocess_textures[postprocess_index].Resource(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		postprocess_barriers.AddTransition(postprocess_textures[!postprocess_index].Resource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		postprocess_barriers.Submit(cmd_list);
		postprocess_index = !postprocess_index;

		if (settings.clouds)
		{
			postprocess_passes[postprocess_index].Begin(cmd_list);

			PassVolumetricClouds(cmd_list);

			postprocess_passes[postprocess_index].End(cmd_list);

			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;

			ResourceBarrierBatch cloud_barriers{};
			postprocess_textures[!postprocess_index].Transition(cloud_barriers,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cloud_barriers.Submit(cmd_list);

			BlurTexture(cmd_list, postprocess_textures[!postprocess_index]);

			cloud_barriers.ReverseTransitions();
			cloud_barriers.Submit(cmd_list);

			postprocess_passes[postprocess_index].Begin(cmd_list);
			CopyTexture(cmd_list, blur_final_texture, EBlendMode::AlphaBlend);
			postprocess_passes[postprocess_index].End(cmd_list);
			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}

		if (settings.fog)
		{
			postprocess_passes[postprocess_index].Begin(cmd_list);
			PassFog(cmd_list);
			postprocess_passes[postprocess_index].End(cmd_list);

			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}

		if (settings.ssr)
		{

			postprocess_passes[postprocess_index].Begin(cmd_list);

			PassSSR(cmd_list);

			postprocess_passes[postprocess_index].End(cmd_list);

			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}

		if (settings.dof)
		{
			ResourceBarrierBatch barrier{};
			postprocess_textures[!postprocess_index].Transition(barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			barrier.Submit(cmd_list);

			BlurTexture(cmd_list, postprocess_textures[!postprocess_index]);

			if (settings.bokeh) PassGenerateBokeh(cmd_list);

			barrier.ReverseTransitions();
			barrier.Submit(cmd_list);

			postprocess_passes[postprocess_index].Begin(cmd_list);

			PassDepthOfField(cmd_list);

			postprocess_passes[postprocess_index].End(cmd_list);
			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}

		if (settings.motion_blur)
		{
			postprocess_passes[postprocess_index].Begin(cmd_list);

			PassMotionBlur(cmd_list);

			postprocess_passes[postprocess_index].End(cmd_list);

			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}
		
		if (settings.bloom)
		{
			PassBloom(cmd_list);

			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}
		
		for (entity light : lights)
		{
			auto const& light_data = lights.get(light);
			if (!light_data.active) continue;

			if (light_data.type == ELightType::Directional)
			{
				
				DrawSun(cmd_list, light);
				if (light_data.god_rays)
				{
					postprocess_barriers.ReverseTransitions();
					postprocess_barriers.Submit(cmd_list);
					postprocess_index = !postprocess_index;
					postprocess_passes[postprocess_index].Begin(cmd_list);
					PassGodRays(cmd_list, light_data);
					postprocess_passes[postprocess_index].End(cmd_list);
				}
				else
				{
					postprocess_passes[postprocess_index].Begin(cmd_list);
					AddTextures(cmd_list, postprocess_textures[!postprocess_index], sun_target);
					postprocess_passes[postprocess_index].End(cmd_list);
				}

				postprocess_barriers.ReverseTransitions();
				postprocess_barriers.Submit(cmd_list);
				postprocess_index = !postprocess_index;
				break;
			}

		}

		if (settings.anti_aliasing & AntiAliasing_TAA)
		{
			postprocess_passes[postprocess_index].Begin(cmd_list);
			PassTAA(cmd_list);
			postprocess_passes[postprocess_index].End(cmd_list);

			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;

			ResourceBarrierBatch taa_barrier{};
			prev_hdr_render_target.Transition(taa_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
			taa_barrier.Submit(cmd_list);
			auto rtv = prev_hdr_render_target.RTV();
			cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

			CopyTexture(cmd_list, postprocess_textures[!postprocess_index]);
			
			cmd_list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
			taa_barrier.ReverseTransitions();
			taa_barrier.Submit(cmd_list);
		}

	}

	void Renderer::PassShadowMapDirectional(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == ELightType::Directional);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Shadow Map Pass - Directional Light");

		auto upload_buffer = gfx->GetUploadBuffer();

		auto const& [V, P] = scene_bounding_sphere ? LightViewProjection_Directional(light, *scene_bounding_sphere, light_bounding_box)
			: LightViewProjection_Directional(light, *camera, light_bounding_box);
		shadow_cbuf_data.lightview = V;
		shadow_cbuf_data.lightviewprojection = V * P;
		shadow_cbuf_data.shadow_map_size = SHADOW_MAP_SIZE;
		shadow_cbuf_data.softness = settings.shadow_softness;
		shadow_cbuf_data.shadow_matrix1 = XMMatrixInverse(nullptr, camera->View()) * shadow_cbuf_data.lightviewprojection;

		shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		shadow_allocation.Update(shadow_cbuf_data);

		ResourceBarrierBatch shadow_map_barrier{};
		shadow_depth_map.Transition(shadow_map_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_map_barrier.Submit(cmd_list);

		shadow_map_pass.Begin(cmd_list);
		{
			LightFrustumCulling(ELightType::Directional);
			PassShadowMapCommon(cmd_list);
		}
		shadow_map_pass.End(cmd_list);
		
		shadow_map_barrier.ReverseTransitions();
		shadow_map_barrier.Submit(cmd_list);
	}
	void Renderer::PassShadowMapSpot(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == ELightType::Spot);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Shadow Map Pass - Spot Light");

		auto upload_buffer = gfx->GetUploadBuffer();

		auto const& [V, P] = LightViewProjection_Spot(light, light_bounding_frustum);
		shadow_cbuf_data.lightview = V;
		shadow_cbuf_data.lightviewprojection = V * P;
		shadow_cbuf_data.shadow_map_size = SHADOW_MAP_SIZE;
		shadow_cbuf_data.softness = settings.shadow_softness;
		shadow_cbuf_data.shadow_matrix1 = XMMatrixInverse(nullptr, camera->View()) * shadow_cbuf_data.lightviewprojection;

		shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		shadow_allocation.Update(shadow_cbuf_data);

		ResourceBarrierBatch shadow_map_barrier{};
		shadow_depth_map.Transition(shadow_map_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_map_barrier.Submit(cmd_list);

		shadow_map_pass.Begin(cmd_list);
		{
			LightFrustumCulling(ELightType::Spot);
			PassShadowMapCommon(cmd_list);
		}
		shadow_map_pass.End(cmd_list);

		shadow_map_barrier.ReverseTransitions();
		shadow_map_barrier.Submit(cmd_list);
	}
	void Renderer::PassShadowMapPoint(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == ELightType::Point);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Shadow Map Pass - Point Light");

		auto upload_buffer = gfx->GetUploadBuffer();

		ResourceBarrierBatch shadow_cubemap_barrier{};
		shadow_depth_cubemap.Transition(shadow_cubemap_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_cubemap_barrier.Submit(cmd_list);

		for (uint32 i = 0; i < shadow_cubemap_passes.size(); ++i)
		{
			auto const& [V, P] = LightViewProjection_Point(light, i, light_bounding_frustum);
			shadow_cbuf_data.lightviewprojection = V * P;
			shadow_cbuf_data.lightview = V;
				
			shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			shadow_allocation.Update(shadow_cbuf_data);

			shadow_cubemap_passes[i].Begin(cmd_list);
			{
				LightFrustumCulling(ELightType::Point);
				PassShadowMapCommon(cmd_list);
			}
			shadow_cubemap_passes[i].End(cmd_list);
		}

		shadow_cubemap_barrier.ReverseTransitions();
		shadow_cubemap_barrier.Submit(cmd_list);
	}
	void Renderer::PassShadowMapCascades(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == ELightType::Directional);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Cascaded Shadow Maps Pass - Directional Light");

		auto upload_buffer = gfx->GetUploadBuffer();

		ResourceBarrierBatch shadow_cascades_barrier{};
		shadow_depth_cascades.Transition(shadow_cascades_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_cascades_barrier.Submit(cmd_list);

		std::array<float32, CASCADE_COUNT> split_distances;
		std::array<XMMATRIX, CASCADE_COUNT> proj_matrices = RecalculateProjectionMatrices(*camera, settings.split_lambda, split_distances);
		std::array<XMMATRIX, CASCADE_COUNT> light_view_projections{};

		for (uint32 i = 0; i < CASCADE_COUNT; ++i)
		{

			auto const& [V, P] = LightViewProjection_Cascades(light, *camera, proj_matrices[i], light_bounding_box);
			light_view_projections[i] = V * P;
			shadow_cbuf_data.lightview = V;
			shadow_cbuf_data.lightviewprojection = light_view_projections[i];
			shadow_cbuf_data.softness = settings.shadow_softness;

			shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			shadow_allocation.Update(shadow_cbuf_data);

			shadow_cascades_passes[i].Begin(cmd_list);
			{
				LightFrustumCulling(ELightType::Directional);
				PassShadowMapCommon(cmd_list);
			}
			shadow_cascades_passes[i].End(cmd_list);
		}
		shadow_cascades_barrier.ReverseTransitions();
		shadow_cascades_barrier.Submit(cmd_list);

		shadow_cbuf_data.shadow_map_size = SHADOW_CASCADE_MAP_SIZE;
		shadow_cbuf_data.shadow_matrix1 = DirectX::XMMatrixInverse(nullptr, camera->View()) * light_view_projections[0];
		shadow_cbuf_data.shadow_matrix2 = DirectX::XMMatrixInverse(nullptr, camera->View()) * light_view_projections[1];
		shadow_cbuf_data.shadow_matrix3 = DirectX::XMMatrixInverse(nullptr, camera->View()) * light_view_projections[2];
		shadow_cbuf_data.split0 = split_distances[0];
		shadow_cbuf_data.split1 = split_distances[1];
		shadow_cbuf_data.split2 = split_distances[2];
		shadow_cbuf_data.softness = settings.shadow_softness;
		shadow_cbuf_data.visualize = static_cast<int>(false);
		
		shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		shadow_allocation.Update(shadow_cbuf_data);
	}
	void Renderer::PassShadowMapCommon(ID3D12GraphicsCommandList4* cmd_list)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		auto shadow_view = reg.view<Mesh, Transform, Visibility>();
		if (!settings.shadow_transparent)
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::DepthMap].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::DepthMap].Get());
			cmd_list->SetGraphicsRootConstantBufferView(1, shadow_allocation.gpu_address);

			for (auto e : shadow_view)
			{
				auto const& visibility = shadow_view.get<Visibility>(e);
				if (visibility.light_visible)
				{
					auto const& transform = shadow_view.get<Transform>(e);
					auto const& mesh = shadow_view.get<Mesh>(e);

					object_cbuf_data.model = transform.current_transform;
					object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

					object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					object_allocation.Update(object_cbuf_data);
					cmd_list->SetGraphicsRootConstantBufferView(0, object_allocation.gpu_address);

					mesh.Draw(cmd_list);
				}
			}
		}
		else
		{
			std::vector<entity> potentially_transparent, not_transparent;
			for (auto e : shadow_view)
			{
				auto const& visibility = shadow_view.get<Visibility>(e);
				if (visibility.light_visible)
				{
					if (auto* p_material = reg.get_if<Material>(e))
					{
						if (p_material->albedo_texture != INVALID_TEXTURE_HANDLE)
							potentially_transparent.push_back(e);
						else not_transparent.push_back(e);
					}
					else not_transparent.push_back(e);
				}
			}

			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::DepthMap].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::DepthMap].Get());
			cmd_list->SetGraphicsRootConstantBufferView(1, shadow_allocation.gpu_address);
			for (auto e : not_transparent)
			{
				auto& transform = shadow_view.get<Transform>(e);
				auto& mesh = shadow_view.get<Mesh>(e);

				object_cbuf_data.model = transform.current_transform;
				object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

				object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				object_allocation.Update(object_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(0, object_allocation.gpu_address);
				mesh.Draw(cmd_list);
			}

			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::DepthMap_Transparent].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::DepthMap_Transparent].Get());
			cmd_list->SetGraphicsRootConstantBufferView(1, shadow_allocation.gpu_address);

			for (auto e : potentially_transparent)
			{
				auto& transform = shadow_view.get<Transform>(e);
				auto& mesh = shadow_view.get<Mesh>(e);
				auto* material = reg.get_if<Material>(e);
				ADRIA_ASSERT(material != nullptr);
				ADRIA_ASSERT(material->albedo_texture != INVALID_TEXTURE_HANDLE);

				object_cbuf_data.model = transform.current_transform;
				object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

				object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				object_allocation.Update(object_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(0, object_allocation.gpu_address);

				D3D12_CPU_DESCRIPTOR_HANDLE albedo_handle = texture_manager.CpuDescriptorHandle(material->albedo_texture);
				OffsetType i = descriptor_allocator->Allocate();

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(i),
					albedo_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(i));

				mesh.Draw(cmd_list);
			}
		}
	}
	
	void Renderer::PassVolumetric(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.volumetric);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Volumetric Lighting Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		if (light.type == ELightType::Directional && !light.casts_shadows)
		{
			ADRIA_LOG(WARNING, "Calling PassVolumetric on a Directional Light that does not cast shadows does not make sense!");
			return;
		}

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Volumetric].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);
		cmd_list->SetGraphicsRootConstantBufferView(2, shadow_allocation.gpu_address);
		cmd_list->SetGraphicsRootConstantBufferView(3, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { depth_target.SRV(), {} };
		uint32 src_range_sizes[] = { 1,1 };

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
		D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };

		switch (light.type)
		{
		case ELightType::Directional:
			if (light.use_cascades)
			{
				cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Volumetric_DirectionalCascades].Get());
				cpu_handles[1] = shadow_depth_cascades.SRV();
			}
			else
			{
				cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Volumetric_Directional].Get());
				cpu_handles[1] = shadow_depth_map.SRV();
			}
			break;
		case ELightType::Spot:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Volumetric_Spot].Get());
			cpu_handles[1] = shadow_depth_map.SRV();
			break;
		case ELightType::Point:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Volumetric_Point].Get());
			cpu_handles[1] = shadow_depth_cubemap.SRV();
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Light Type!");
		}

		device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

	}
	void Renderer::PassForwardCommon(ID3D12GraphicsCommandList4* cmd_list, bool transparent)
	{
		auto forward_view = reg.view<Mesh, Transform, Material, Forward, Visibility>();
		std::unordered_map<EPipelineStateObject, std::vector<entity>> entities_group;

		for (auto e : forward_view)
		{
			auto const& material = reg.get<Material>(e);
			auto const& forward = reg.get<Forward>(e);

			if (forward.transparent == transparent)
				entities_group[material.pso].push_back(e);
		}

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		if (entities_group.empty()) return;

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Forward].Get());
		
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		
		for (auto const& [pso, entities] : entities_group)
		{
			cmd_list->SetPipelineState(pso_map[pso].Get());

			for (auto e : entities)
			{
				auto [mesh, transform, material, visibility] = forward_view.get<Mesh, Transform, Material, Visibility>(e);

				if (!visibility.camera_visible) continue;

				object_cbuf_data.model = transform.current_transform;
				object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, object_cbuf_data.model));

				object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				object_allocation.Update(object_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

				material_cbuf_data.diffuse = material.diffuse;
				
				DynamicAllocation material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				material_allocation.Update(material_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

				D3D12_CPU_DESCRIPTOR_HANDLE diffuse_handle = texture_manager.CpuDescriptorHandle(material.albedo_texture);
				uint32 src_range_size = 1;

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);

				device->CopyDescriptorsSimple(1, dst_descriptor, diffuse_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

				mesh.Draw(cmd_list);
			}

		}

	}
	void Renderer::PassSky(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Sky Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		ObjectCBuffer object_cbuf_data{};
		object_cbuf_data.model = DirectX::XMMatrixTranslationFromVector(camera->Position());
		object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		object_allocation.Update(object_cbuf_data);
		
		switch (settings.sky_type)
		{
		case ESkyType::Skybox:
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Skybox].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Skybox].Get());

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

			auto skybox_view = reg.view<Skybox>();
			for (auto e : skybox_view)
			{
				auto const& skybox = skybox_view.get(e);
				if (!skybox.active) continue;

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				ADRIA_ASSERT(skybox.cubemap_texture != INVALID_TEXTURE_HANDLE);
				D3D12_CPU_DESCRIPTOR_HANDLE texture_handle = texture_manager.CpuDescriptorHandle(skybox.cubemap_texture);

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), texture_handle,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
			}
			break;
		}
		case ESkyType::UniformColor:
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Sky].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::UniformColorSky].Get());
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
			cmd_list->SetGraphicsRootConstantBufferView(2, weather_cbuffer.View(backbuffer_index).BufferLocation);
			break;
		}
		case ESkyType::HosekWilkie:
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Sky].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::HosekWilkieSky].Get());
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
			cmd_list->SetGraphicsRootConstantBufferView(2, weather_cbuffer.View(backbuffer_index).BufferLocation);
			break;
		}
		default:
			ADRIA_ASSERT(false);
		}

		cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cube_vb->Bind(cmd_list, 0);
		cube_ib->Bind(cmd_list);
		cmd_list->DrawIndexedInstanced(cube_ib->IndexCount(), 1, 0, 0, 0);

	}
	void Renderer::UpdateOcean(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Ocean Update Pass");
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		if (reg.size<Ocean>() == 0) return;

		if (settings.ocean_color_changed)
		{
			auto ocean_view = reg.view<Ocean, Material>();
			for (auto e : ocean_view)
			{
				auto& material = ocean_view.get<Material>(e);
				material.diffuse = XMFLOAT3(settings.ocean_color);
			}
		}

		if (settings.recreate_initial_spectrum)
		{
			ResourceBarrierBatch initial_spectrum_barrier{};
			initial_spectrum_barrier.AddTransition(ocean_initial_spectrum.Resource(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			initial_spectrum_barrier.Submit(cmd_list);

			cmd_list->SetComputeRootSignature(rs_map[ERootSignature::InitialSpectrum].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::InitialSpectrum].Get());
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			device->CopyDescriptorsSimple(1, dst_descriptor, ocean_initial_spectrum.UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch(RESOLUTION / 32, RESOLUTION / 32, 1);

			initial_spectrum_barrier.ReverseTransitions();
			initial_spectrum_barrier.Submit(cmd_list);

			settings.recreate_initial_spectrum = false;
		}

		//phase
		{
			ResourceBarrierBatch phase_barriers{};
			phase_barriers.AddTransition(ping_pong_phase_textures[pong_phase].Resource(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			phase_barriers.AddTransition(ping_pong_phase_textures[!pong_phase].Resource(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			phase_barriers.Submit(cmd_list);

			cmd_list->SetComputeRootSignature(rs_map[ERootSignature::Phase].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Phase].Get());
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index),
				ping_pong_phase_textures[pong_phase].SRV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

			descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index),
				ping_pong_phase_textures[!pong_phase].UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch(RESOLUTION / 32, RESOLUTION / 32, 1);

			pong_phase = !pong_phase;
		}

		//spectrum
		{
			cmd_list->SetComputeRootSignature(rs_map[ERootSignature::Spectrum].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Spectrum].Get());
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			D3D12_CPU_DESCRIPTOR_HANDLE srvs[] = { ping_pong_phase_textures[pong_phase].SRV(), ocean_initial_spectrum.SRV() };
			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(srvs));
			auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			uint32 dst_range_sizes[] = { ARRAYSIZE(srvs) };
			uint32 src_range_sizes[] = { 1, 1 };
			device->CopyDescriptors(ARRAYSIZE(dst_range_sizes), &dst_descriptor, dst_range_sizes, ARRAYSIZE(src_range_sizes), srvs, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

			descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index),
				ping_pong_spectrum_textures[pong_spectrum].UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

			cmd_list->Dispatch(RESOLUTION / 32, RESOLUTION / 32, 1);
		}

		//fft
		{
			DECLSPEC_ALIGN(16) struct FFTCBuffer
			{
				uint32 seq_count;
				uint32 subseq_count;
			};
			static FFTCBuffer fft_cbuf_data{ .seq_count = RESOLUTION };
			DynamicAllocation fft_cbuffer_allocation{};

			cmd_list->SetComputeRootSignature(rs_map[ERootSignature::FFT].Get());
			{
				cmd_list->SetPipelineState(pso_map[EPipelineStateObject::FFT_Horizontal].Get());
				for (uint32 p = 1; p < RESOLUTION; p <<= 1)
				{
					ResourceBarrierBatch fft_barriers{};
					fft_barriers.AddTransition(ping_pong_spectrum_textures[!pong_spectrum].Resource(),
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					fft_barriers.AddTransition(ping_pong_spectrum_textures[pong_spectrum].Resource(),
						D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					fft_barriers.Submit(cmd_list);
					
					fft_cbuf_data.subseq_count = p;
					fft_cbuffer_allocation = upload_buffer->Allocate(GetCBufferSize<FFTCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					fft_cbuffer_allocation.Update(fft_cbuf_data);
					cmd_list->SetComputeRootConstantBufferView(0, fft_cbuffer_allocation.gpu_address);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index),
						ping_pong_spectrum_textures[pong_spectrum].SRV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

					device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 1),
						ping_pong_spectrum_textures[!pong_spectrum].UAV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index + 1));
					cmd_list->Dispatch(RESOLUTION, 1, 1);

					pong_spectrum = !pong_spectrum;
				}
			}

			{
				cmd_list->SetPipelineState(pso_map[EPipelineStateObject::FFT_Vertical].Get());
				for (uint32 p = 1; p < RESOLUTION; p <<= 1)
				{
					ResourceBarrierBatch fft_barriers{};
					fft_barriers.AddTransition(ping_pong_spectrum_textures[!pong_spectrum].Resource(),
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					fft_barriers.AddTransition(ping_pong_spectrum_textures[pong_spectrum].Resource(),
						D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					fft_barriers.Submit(cmd_list);

					fft_cbuf_data.subseq_count = p;
					fft_cbuffer_allocation = upload_buffer->Allocate(GetCBufferSize<FFTCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					fft_cbuffer_allocation.Update(fft_cbuf_data);
					cmd_list->SetComputeRootConstantBufferView(0, fft_cbuffer_allocation.gpu_address);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index),
						ping_pong_spectrum_textures[pong_spectrum].SRV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

					device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 1),
						ping_pong_spectrum_textures[!pong_spectrum].UAV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index + 1));
					cmd_list->Dispatch(RESOLUTION, 1, 1);

					pong_spectrum = !pong_spectrum;

				}
			}
		}

		//normals
		{
			ResourceBarrierBatch normal_barrier{};
			normal_barrier.AddTransition(ocean_normal_map.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			normal_barrier.Submit(cmd_list);

			cmd_list->SetComputeRootSignature(rs_map[ERootSignature::OceanNormalMap].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::OceanNormalMap].Get());
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index),
				ping_pong_spectrum_textures[pong_spectrum].SRV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

			descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index),
				ocean_normal_map.UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch(RESOLUTION / 32, RESOLUTION / 32, 1);

			normal_barrier.ReverseTransitions();
			normal_barrier.Submit(cmd_list);
		}
	}
	void Renderer::PassOcean(ID3D12GraphicsCommandList4* cmd_list)
	{
		if (reg.size<Ocean>() == 0) return;

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();
		
		auto skyboxes = reg.view<Skybox>();
		D3D12_CPU_DESCRIPTOR_HANDLE skybox_handle = null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT);
		for (auto skybox : skyboxes)
		{
			auto const& _skybox = skyboxes.get(skybox);
			if (_skybox.active)
			{
				skybox_handle = texture_manager.CpuDescriptorHandle(_skybox.cubemap_texture);
				break;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE displacement_handle = ping_pong_spectrum_textures[!pong_spectrum].SRV();

		if (settings.ocean_tesselation)
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::OceanLOD].Get());
			cmd_list->SetPipelineState(
				settings.ocean_wireframe ? pso_map[EPipelineStateObject::OceanLOD_Wireframe].Get() : pso_map[EPipelineStateObject::OceanLOD].Get());
		}
		else
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Ocean].Get());
			cmd_list->SetPipelineState(
				settings.ocean_wireframe ? pso_map[EPipelineStateObject::Ocean_Wireframe].Get() : pso_map[EPipelineStateObject::Ocean].Get());
		}
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(3, weather_cbuffer.View(backbuffer_index).BufferLocation);

		auto ocean_chunk_view = reg.view<Mesh, Material, Transform, Visibility, Ocean>();
		for (auto ocean_chunk : ocean_chunk_view)
		{
			auto [mesh, material, transform, visibility] = ocean_chunk_view.get<const Mesh, const Material, const Transform, const Visibility>(ocean_chunk);

			if (visibility.camera_visible)
			{
				object_cbuf_data.model = transform.current_transform;
				object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);
				object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				object_allocation.Update(object_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

				material_cbuf_data.diffuse = material.diffuse;
				material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				material_allocation.Update(material_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, displacement_handle,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));

				descriptor_index = descriptor_allocator->AllocateRange(3);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { ocean_normal_map.SRV(), skybox_handle, texture_manager.CpuDescriptorHandle(foam_handle) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
				UINT src_range_sizes[] = { 1, 1, 1 };
				UINT dst_range_sizes[] = { 3 };
				device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(5, descriptor_allocator->GetGpuHandle(descriptor_index));

				settings.ocean_tesselation ? mesh.Draw(cmd_list, D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST) : mesh.Draw(cmd_list);
			}
		}
	}
	void Renderer::PassParticles(ID3D12GraphicsCommandList4* cmd_list)
	{
		if (reg.size<Emitter>() == 0) return;
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Particles Pass");
		SCOPED_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, EProfilerBlock::ParticlesPass, profiler_settings.profile_particles_pass);

		particle_pass.Begin(cmd_list, true);
		auto emitters = reg.view<Emitter>();
		for (auto emitter : emitters)
		{
			Emitter const& emitter_params = emitters.get(emitter);
			particle_renderer.Render(cmd_list, emitter_params, depth_target.SRV(), texture_manager.CpuDescriptorHandle(emitter_params.particle_texture));
		}
		particle_pass.End(cmd_list, true);
	}

	void Renderer::PassLensFlare(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.lens_flare);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Lens Flare Pass");

		if (light.type != ELightType::Directional)
		{
			ADRIA_LOG(WARNING, "Using Lens Flare on a Non-Directional Light Source");
			return;
		}
		
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		lens_flare_textures.push_back(depth_target.SRV());

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::LensFlare].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::LensFlare].Get());

		light_allocation = upload_buffer->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		
		{
			auto camera_position = camera->Position();
			XMVECTOR light_position = light.type == ELightType::Directional ?
				XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position)))
				: light.position;

			DirectX::XMVECTOR light_pos_h = XMVector4Transform(light_position, camera->ViewProj());
			DirectX::XMFLOAT4 light_pos{};
			DirectX::XMStoreFloat4(&light_pos, light_pos_h);
			light_cbuf_data.ss_position = XMVectorSet(0.5f * light_pos.x / light_pos.w + 0.5f, -0.5f * light_pos.y / light_pos.w + 0.5f, light_pos.z / light_pos.w, 1.0f);
		}

		light_allocation.Update(light_cbuf_data);

		cmd_list->SetGraphicsRootConstantBufferView(0, light_allocation.gpu_address);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(8);
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1, 1, 1, 1, 1, 1 };
		uint32 dst_range_sizes[] = { 8 };
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_range_sizes), lens_flare_textures.data(), src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		cmd_list->DrawInstanced(7, 1, 0, 0);

		lens_flare_textures.pop_back();
	}
	void Renderer::PassVolumetricClouds(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.clouds);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Volumetric Clouds Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		
		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Clouds].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Clouds].Get());
		
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, weather_cbuffer.View(backbuffer_index).BufferLocation);
		
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(4);
		
		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { clouds_textures[0], clouds_textures[1], clouds_textures[2], depth_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1, 1 };
		uint32 dst_range_sizes[] = { 4 };
		
		
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		
		cmd_list->DrawInstanced(4, 1, 0, 0);


	}
	void Renderer::PassSSR(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.ssr);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "SSR Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::SSR].Get());

		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::SSR].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0].SRV(), postprocess_textures[!postprocess_index].SRV(), depth_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1 };
		uint32 dst_range_sizes[] = { 3 };

		
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassDepthOfField(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Depth of Field Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::DOF].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::DOF].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index].SRV(), blur_final_texture.SRV(), depth_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1 };
		uint32 dst_range_sizes[] = { 3 };

		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

		if (settings.bokeh) PassDrawBokeh(cmd_list);
	}
	void Renderer::PassGenerateBokeh(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof && settings.bokeh);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Bokeh Generation Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		D3D12_RESOURCE_BARRIER prereset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,  D3D12_RESOURCE_STATE_COPY_DEST);
		cmd_list->ResourceBarrier(1, &prereset_barrier);
		cmd_list->CopyBufferRegion(bokeh->CounterBuffer(), 0, counter_reset_buffer.Get(), 0, sizeof(uint32));
		D3D12_RESOURCE_BARRIER postreset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(),D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmd_list->ResourceBarrier(1, &postreset_barrier);

 		cmd_list->SetComputeRootSignature(rs_map[ERootSignature::BokehGenerate].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::BokehGenerate].Get());
		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(2, compute_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

		D3D12_RESOURCE_BARRIER dispatch_barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(depth_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(bokeh->Buffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(dispatch_barriers), dispatch_barriers);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index].SRV(), depth_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1 };
		uint32 dst_range_sizes[] = { 2 };
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

		D3D12_CPU_DESCRIPTOR_HANDLE bokeh_uav = bokeh->UAV();
		descriptor_index = descriptor_allocator->Allocate();

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), bokeh_uav,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->Dispatch((uint32)std::ceil(width / 32.0f), (uint32)std::ceil(height / 32.0f), 1);

		CD3DX12_RESOURCE_BARRIER precopy_barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh->Buffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(precopy_barriers), precopy_barriers);

		cmd_list->CopyBufferRegion(bokeh_indirect_draw_buffer.Get(), 0, bokeh->CounterBuffer(), 0, bokeh->CounterBuffer()->GetDesc().Width);

		CD3DX12_RESOURCE_BARRIER postcopy_barriers[] = 
		{
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(postcopy_barriers), postcopy_barriers);
	}
	void Renderer::PassDrawBokeh(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof && settings.bokeh);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Bokeh Draw Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		D3D12_CPU_DESCRIPTOR_HANDLE bokeh_descriptor{};
		switch (settings.bokeh_type)
		{
		case EBokehType::Hex:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(hex_bokeh_handle);
			break;
		case EBokehType::Oct:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(oct_bokeh_handle);
			break;
		case EBokehType::Circle:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(circle_bokeh_handle);
			break;
		case EBokehType::Cross:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(cross_bokeh_handle);
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Bokeh Type");
		}

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Bokeh].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Bokeh].Get());

		OffsetType i = descriptor_allocator->AllocateRange(2);

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(i),
			bokeh->SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(i + 1),
			bokeh_descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(i));
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(i + 1));

		cmd_list->IASetVertexBuffers(0, 0, nullptr);
		cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		cmd_list->ExecuteIndirect(bokeh_command_signature.Get(), 1, bokeh_indirect_draw_buffer.Get(), 0,
			nullptr, 0);

	}
	void Renderer::PassBloom(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.bloom);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Bloom Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		D3D12_RESOURCE_BARRIER extract_barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(bloom_extract_texture.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[!postprocess_index].Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(extract_barriers), extract_barriers);

		cmd_list->SetComputeRootSignature(rs_map[ERootSignature::BloomExtract].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::BloomExtract].Get());
		cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);
		
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = postprocess_textures[!postprocess_index].SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
		
		++descriptor_index;
		cpu_descriptor = bloom_extract_texture.UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		
		cmd_list->Dispatch((uint32)std::ceil(postprocess_textures[!postprocess_index].Width() / 32), 
						   (uint32)std::ceil(postprocess_textures[!postprocess_index].Height() / 32), 1);

		D3D12_RESOURCE_BARRIER combine_barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(bloom_extract_texture.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[postprocess_index].Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		};

		cmd_list->ResourceBarrier(ARRAYSIZE(combine_barriers), combine_barriers);

		GenerateMips(cmd_list, bloom_extract_texture);

		cmd_list->SetComputeRootSignature(rs_map[ERootSignature::BloomCombine].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::BloomCombine].Get());

		descriptor_index = descriptor_allocator->AllocateRange(3);
		cpu_descriptor = postprocess_textures[!postprocess_index].SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//++descriptor_index;
		cpu_descriptor = bloom_extract_texture.SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 1), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));

		descriptor_index += 2;
		cpu_descriptor = postprocess_textures[postprocess_index].UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->Dispatch((uint32)std::ceil(postprocess_textures[!postprocess_index].Width() / 32),
			(uint32)std::ceil(postprocess_textures[!postprocess_index].Height() / 32), 1);


		D3D12_RESOURCE_BARRIER final_barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[!postprocess_index].Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[postprocess_index].Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET),
		};

		cmd_list->ResourceBarrier(ARRAYSIZE(final_barriers), final_barriers);

	}
	void Renderer::PassMotionBlur(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.motion_blur);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Motion Blur Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::MotionBlur].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::MotionBlur].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index].SRV(), velocity_buffer.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1 };
		uint32 dst_range_sizes[] = { 2 };

		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassFog(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.fog);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Fog Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Fog].Get());

		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Fog].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = {postprocess_textures[!postprocess_index].SRV(), depth_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1 };
		uint32 dst_range_sizes[] = { 2 };

		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	
	void Renderer::PassGodRays(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.god_rays);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "God Rays Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		if (light.type != ELightType::Directional)
		{
			ADRIA_LOG(WARNING, "Using God Rays on a Non-Directional Light Source");
			return;
		}

		
		LightCBuffer light_cbuf_data{};
		{
			light_cbuf_data.godrays_decay = light.godrays_decay;
			light_cbuf_data.godrays_density = light.godrays_density;
			light_cbuf_data.godrays_exposure = light.godrays_exposure;
			light_cbuf_data.godrays_weight = light.godrays_weight;

			auto camera_position = camera->Position();
			XMVECTOR light_position = light.type == ELightType::Directional ?
				XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position)))
				: light.position;

			DirectX::XMVECTOR light_pos_h = XMVector4Transform(light_position, camera->ViewProj());
			DirectX::XMFLOAT4 light_pos{};
			DirectX::XMStoreFloat4(&light_pos, light_pos_h);
			light_cbuf_data.ss_position = XMVectorSet(0.5f * light_pos.x / light_pos.w + 0.5f, -0.5f * light_pos.y / light_pos.w + 0.5f, light_pos.z / light_pos.w, 1.0f);

			static const float32 f_max_sun_dist = 1.3f;
			float32 f_max_dist = (std::max)(abs(XMVectorGetX(light_cbuf_data.ss_position)), abs(XMVectorGetY(light_cbuf_data.ss_position)));
			if (f_max_dist >= 1.0f)
				light_cbuf_data.color = XMVector3Transform(light.color, XMMatrixScaling((f_max_sun_dist - f_max_dist), (f_max_sun_dist - f_max_dist), (f_max_sun_dist - f_max_dist)));
			else light_cbuf_data.color = light.color;
		}
		
		light_allocation = upload_buffer->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		light_allocation.Update(light_cbuf_data);

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::GodRays].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::GodRays].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, light_allocation.gpu_address);

		OffsetType descriptor_index = descriptor_allocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = sun_target.SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassVelocityBuffer(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Velocity Buffer Pass");

		if (!settings.motion_blur && !(settings.anti_aliasing & AntiAliasing_TAA)) return;

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		ResourceBarrierBatch velocity_barrier{};
		velocity_barrier.AddTransition(velocity_buffer.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		velocity_barrier.Submit(cmd_list);

		velocity_buffer_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::VelocityBuffer].Get());
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::VelocityBuffer].Get());

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);
			
			OffsetType descriptor_index = descriptor_allocator->AllocateRange(1);
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), 
				depth_target.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmd_list->DrawInstanced(4, 1, 0, 0);
		}
		velocity_buffer_pass.End(cmd_list);

		velocity_barrier.ReverseTransitions();
		velocity_barrier.Submit(cmd_list);
	}

	void Renderer::PassToneMap(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Tone Map Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::ToneMap].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::ToneMap].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = postprocess_textures[!postprocess_index].SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

	}
	void Renderer::PassFXAA(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.anti_aliasing & AntiAliasing_FXAA);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "FXAA Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		ResourceBarrierBatch fxaa_barrier{};
		ldr_render_target.Transition(fxaa_barrier, D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		fxaa_barrier.Submit(cmd_list);

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::FXAA].Get());

		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::FXAA].Get());

		OffsetType descriptor_index = descriptor_allocator->Allocate();

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), ldr_render_target.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);

		fxaa_barrier.ReverseTransitions();
		fxaa_barrier.Submit(cmd_list);
	}
	void Renderer::PassTAA(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.anti_aliasing & AntiAliasing_TAA);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "TAA Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::TAA].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::TAA].Get());

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), postprocess_textures[!postprocess_index].SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 1), prev_hdr_render_target.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 2), velocity_buffer.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);
	}

	void Renderer::DrawSun(ID3D12GraphicsCommandList4* cmd_list, tecs::entity sun)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Sun Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();
		auto upload_buffer = gfx->GetUploadBuffer();

		ResourceBarrierBatch sun_barrier{};
		sun_barrier.AddTransition(sun_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		sun_barrier.AddTransition(depth_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		sun_barrier.Submit(cmd_list);


		D3D12_CPU_DESCRIPTOR_HANDLE rtv = sun_target.RTV();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = depth_target.DSV();
		float32 black[4] = { 0.0f };
		cmd_list->ClearRenderTargetView(rtv, black, 0, nullptr);
		cmd_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Forward].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Sun].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		{
			auto [transform, mesh, material] = reg.get<Transform, Mesh, Material>(sun);

			object_cbuf_data.model = transform.current_transform;
			object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, object_cbuf_data.model));

			object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			object_allocation.Update(object_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

			material_cbuf_data.diffuse = material.diffuse;

			DynamicAllocation material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			material_allocation.Update(material_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

			D3D12_CPU_DESCRIPTOR_HANDLE diffuse_handle = texture_manager.CpuDescriptorHandle(material.albedo_texture);
			uint32 src_range_size = 1;

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);

			device->CopyDescriptorsSimple(1, dst_descriptor, diffuse_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

			mesh.Draw(cmd_list);

		}
		cmd_list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

		sun_barrier.ReverseTransitions();
		sun_barrier.Submit(cmd_list);
	}
	void Renderer::BlurTexture(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture)
	{
		
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		ResourceBarrierBatch barrier{};

		blur_intermediate_texture.Transition(barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		barrier.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(rs_map[ERootSignature::Blur].Get());

		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Blur_Horizontal].Get());

		cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = texture.SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

		++descriptor_index;
		cpu_descriptor = blur_intermediate_texture.UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->Dispatch((uint32)std::ceil(texture.Width() / 1024.0f), texture.Height(), 1);

		barrier.ReverseTransitions();
		blur_final_texture.Transition(barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		barrier.Submit(cmd_list);

		//vertical pass

		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Blur_Vertical].Get());

		descriptor_index = descriptor_allocator->AllocateRange(2);

		cpu_descriptor = blur_intermediate_texture.SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

		++descriptor_index;
		cpu_descriptor = blur_final_texture.UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));


		cmd_list->Dispatch(texture.Width(), (UINT)std::ceil(texture.Height() / 1024.0f), 1);

		barrier.Clear();
		blur_final_texture.Transition(barrier, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		barrier.Submit(cmd_list);
	}
	void Renderer::CopyTexture(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture, EBlendMode mode)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Copy].Get());

		switch (mode)
		{
		case EBlendMode::None:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Copy].Get());
			break;
		case EBlendMode::AlphaBlend:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Copy_AlphaBlend].Get());
			break;
		case EBlendMode::AdditiveBlend:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Copy_AdditiveBlend].Get());
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
		}

		OffsetType descriptor_index = descriptor_allocator->Allocate();

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), texture.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::GenerateMips(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& _texture,
		D3D12_RESOURCE_STATES start_state, D3D12_RESOURCE_STATES end_state)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		ID3D12Resource* texture = _texture.Resource();

		//Set root signature, pso and descriptor heap
		cmd_list->SetComputeRootSignature(rs_map[ERootSignature::GenerateMips].Get());
		cmd_list->SetPipelineState(pso_map[EPipelineStateObject::GenerateMips].Get());

		//Prepare the shader resource view description for the source texture
		D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc = {};
		src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		//Prepare the unordered access view description for the destination texture
		D3D12_UNORDERED_ACCESS_VIEW_DESC dst_uav_desc = {};
		dst_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();
		UINT const mipmap_levels = tex_desc.MipLevels;

		if (start_state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		{
			auto transition_barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cmd_list->ResourceBarrier(1, &transition_barrier);
		}

		OffsetType i{};
		for (UINT top_mip = 0; top_mip < mipmap_levels - 1; top_mip++)
		{
			//Get mipmap dimensions
			UINT dst_width = (std::max)((UINT)tex_desc.Width >> (top_mip + 1), 1u);
			UINT dst_height = (std::max)(tex_desc.Height >> (top_mip + 1), 1u);


			i = descriptor_allocator->AllocateRange(2);
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle1 = descriptor_allocator->GetCpuHandle(i);
			D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle1 = descriptor_allocator->GetGpuHandle(i);

			src_srv_desc.Format = tex_desc.Format;
			src_srv_desc.Texture2D.MipLevels = 1;
			src_srv_desc.Texture2D.MostDetailedMip = top_mip;
			device->CreateShaderResourceView(texture, &src_srv_desc, cpu_handle1);


			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle2 = descriptor_allocator->GetCpuHandle(i + 1);
			D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle2 = descriptor_allocator->GetGpuHandle(i + 1);
			dst_uav_desc.Format = tex_desc.Format;
			dst_uav_desc.Texture2D.MipSlice = top_mip + 1;
			device->CreateUnorderedAccessView(texture, nullptr, &dst_uav_desc, cpu_handle2);
			//Pass the destination texture pixel size to the shader as constants
			cmd_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_width).Uint, 0);
			cmd_list->SetComputeRoot32BitConstant(0, DWParam(1.0f / dst_height).Uint, 1);

			cmd_list->SetComputeRootDescriptorTable(1, gpu_handle1);
			cmd_list->SetComputeRootDescriptorTable(2, gpu_handle2);

			//Dispatch the compute shader with one thread per 8x8 pixels
			cmd_list->Dispatch((std::max)(dst_width / 8u, 1u), (std::max)(dst_height / 8u, 1u), 1);

			//Wait for all accesses to the destination texture UAV to be finished before generating the next mipmap, as it will be the source texture for the next mipmap
			auto uav_barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture);
			cmd_list->ResourceBarrier(1, &uav_barrier);
		}

		if (end_state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		{
			auto transition_barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				end_state);
			cmd_list->ResourceBarrier(1, &transition_barrier);
		}

	}
	void Renderer::AddTextures(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture1, Texture2D const& texture2, EBlendMode mode)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[ERootSignature::Add].Get());

		switch (mode)
		{
		case EBlendMode::None:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Add].Get());
			break;
		case EBlendMode::AlphaBlend:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Add_AlphaBlend].Get());
			break;
		case EBlendMode::AdditiveBlend:
			cmd_list->SetPipelineState(pso_map[EPipelineStateObject::Add_AdditiveBlend].Get());
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
		}


		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), texture1.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 1), texture2.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
}
