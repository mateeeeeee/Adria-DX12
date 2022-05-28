
#include "../Tasks/TaskSystem.h"
#include "../Utilities/Random.h"
#include "Renderer.h"
#include <algorithm>
#include <iterator>
#include "Renderer.h"
#include "Camera.h"
#include "Components.h"
#include "RootSigPSOManager.h"
#include "SkyModel.h"
#include "../Core/Window.h"
#include "../Utilities/Timer.h"
#include "../Math/Constants.h"
#include "../Logging/Logger.h"
#include "../Editor/GUI.h"
#include "../Graphics/DWParam.h"
#include "../RenderGraph/RenderGraph.h"
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
			static float32 const lightDistanceFactor = 1.0f;

			std::array<XMMATRIX, CASCADE_COUNT> lightViewProjectionMatrices{};

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

	Renderer::Renderer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height)
		: reg(reg), gfx(gfx), width(width), height(height), texture_manager(gfx, 1000), backbuffer_count(gfx->BackbufferCount()),
		frame_cbuffer(gfx->GetDevice(), backbuffer_count), postprocess_cbuffer(gfx->GetDevice(), backbuffer_count),
		compute_cbuffer(gfx->GetDevice(), backbuffer_count), weather_cbuffer(gfx->GetDevice(), backbuffer_count),
		clusters(gfx, StructuredBufferDesc<ClusterAABB>(CLUSTER_COUNT)),
		light_counter(gfx, StructuredBufferDesc<uint32>(1)),
		light_list(gfx, StructuredBufferDesc<uint32>(CLUSTER_COUNT * CLUSTER_MAX_LIGHTS)),
		light_grid(gfx, StructuredBufferDesc<LightGrid>(CLUSTER_COUNT)),
		bokeh_counter(gfx, CounterBufferDesc()),
		gpu_profiler(gfx), particle_renderer(gfx), picker(gfx), ray_tracer(reg, gfx, width, height)
	{
		RootSigPSOManager::Initialize(gfx->GetDevice());
		CreateDescriptorHeaps();
		CreateResolutionDependentResources(width, height);
		CreateOtherResources();
		CreateRenderPasses(width, height);
	}
	Renderer::~Renderer()
	{
		gfx->WaitForGPU();
		RootSigPSOManager::Destroy();
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

		ray_tracer.Update(RayTracingSettings{
			.dt = dt, .ao_radius = settings.rtao_radius }
		);
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
		auto gbuf_cmd_list = gfx->GetNewGraphicsCommandList();
		auto deferred_cmd_list = gfx->GetNewGraphicsCommandList();
		auto postprocess_cmd_list = gfx->GetNewGraphicsCommandList();

		auto gbuf_ambient_fut = TaskSystem::Submit([this, gbuf_cmd_list]()
		{
			D3D12_RESOURCE_BARRIER picking_barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
					CD3DX12_RESOURCE_BARRIER::Transition(gbuffer[0]->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				};

			gbuf_cmd_list->ResourceBarrier(ARRAYSIZE(picking_barriers), picking_barriers);
			PassPicking(gbuf_cmd_list);

			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(hdr_render_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE),
				CD3DX12_RESOURCE_BARRIER::Transition(gbuffer[0]->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			};

			gbuf_cmd_list->ResourceBarrier(ARRAYSIZE(barriers), barriers);
			PassGBuffer(gbuf_cmd_list);
		}
		);

		auto deferred_fut = TaskSystem::Submit([this, deferred_cmd_list]()
		{
			D3D12_RESOURCE_BARRIER depth_barrier[1] = {};
			depth_barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			deferred_cmd_list->ResourceBarrier(ARRAYSIZE(depth_barrier), depth_barrier);
			PassDecals(deferred_cmd_list);

			switch (settings.ambient_occlusion)
			{
			case EAmbientOcclusion::SSAO:
			{
				PassSSAO(deferred_cmd_list);
				break;
			}
			case EAmbientOcclusion::HBAO:
			{
				PassHBAO(deferred_cmd_list);
				break;
			}
			case EAmbientOcclusion::RTAO:
			{
				ResourceBarrierBatch rtao_barriers{};
				rtao_barriers.AddTransition(gbuffer[0]->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				rtao_barriers.AddTransition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				rtao_barriers.Submit(deferred_cmd_list);
				PassRTAO(deferred_cmd_list);
				rtao_barriers.ReverseTransitions();
				rtao_barriers.Submit(deferred_cmd_list);
				break;
			}
			case EAmbientOcclusion::None:
			default:
				break;
			}
			
			PassAmbient(deferred_cmd_list);
			PassDeferredLighting(deferred_cmd_list);

			if (settings.use_tiled_deferred) PassDeferredTiledLighting(deferred_cmd_list);
			else if (settings.use_clustered_deferred) PassDeferredClusteredLighting(deferred_cmd_list);

			depth_barrier[0] = CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			deferred_cmd_list->ResourceBarrier(ARRAYSIZE(depth_barrier), depth_barrier);

			PassForward(deferred_cmd_list);
			PassParticles(deferred_cmd_list);
		}
		);

		auto postprocess_fut = TaskSystem::Submit([this, postprocess_cmd_list]()
		{
			
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(hdr_render_target->GetNative(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
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
			offscreen_barrier.AddTransition(offscreen_ldr_target->GetNative(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
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
			offscreen_barrier.AddTransition(offscreen_ldr_target->GetNative(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
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
	void Renderer::OnSceneInitialized()
	{
		UINT tex2darray_size = (UINT)texture_manager.handle;
		gfx->ReserveOnlineDescriptors(tex2darray_size);

		ID3D12Device* device = gfx->GetDevice();
		RingOnlineDescriptorAllocator* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		device->CopyDescriptorsSimple(tex2darray_size, descriptor_allocator->GetFirstHandle(),
			texture_manager.texture_srv_heap->GetFirstHandle(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		UploadData();
		texture_manager.SetMipMaps(false);
		//lens flare
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

		if (ray_tracer.IsSupported())
		{
			ray_tracer.OnSceneInitialized();
			ResourceBarrierBatch barriers{};
			for (auto e : reg.view<Skybox>())
			{
				auto const& skybox = reg.get<Skybox>(e);
				if (skybox.used_in_rt) texture_manager.TransitionTexture(skybox.cubemap_texture, barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}
			barriers.Submit(gfx->GetDefaultCommandList());
		}
	}
	TextureManager& Renderer::GetTextureManager()
	{
		return texture_manager;
	}
	std::vector<std::string> Renderer::GetProfilerResults(bool log)
{
		return gpu_profiler.GetProfilerResults(gfx->GetDefaultCommandList(), log);
	}
	PickingData Renderer::GetPickingData() const
	{
		return picking_data;
	}
	Texture const& Renderer::GetOffscreenTexture() const
	{
		return *offscreen_ldr_target;
	}

	void Renderer::CreateDescriptorHeaps()
	{
		ID3D12Device* device = gfx->GetDevice();

		null_srv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, NULL_HEAP_SIZE));
		D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
		null_srv_desc.Texture2D.MostDetailedMip = 0;
		null_srv_desc.Texture2D.MipLevels = -1;
		null_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		null_srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_srv_heap->GetHandle(TEXTURE2D_SLOT));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_srv_heap->GetHandle(TEXTURECUBE_SLOT));
		null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		device->CreateShaderResourceView(nullptr, &null_srv_desc, null_srv_heap->GetHandle(TEXTURE2DARRAY_SLOT));

		D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc{};
		null_uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		null_uav_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		device->CreateUnorderedAccessView(nullptr,nullptr, &null_uav_desc, null_srv_heap->GetHandle(RWTEXTURE2D_SLOT));
	}
	void Renderer::CreateResolutionDependentResources(uint32 width, uint32 height)
	{
		D3D12_CLEAR_VALUE rtv_clear_value{};
		rtv_clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rtv_clear_value.Color[0] = 0.0f;
		rtv_clear_value.Color[1] = 0.0f;
		rtv_clear_value.Color[2] = 0.0f;
		rtv_clear_value.Color[3] = 0.0f;
		//main render target
		{
			TextureDesc render_target_desc{};
			render_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			render_target_desc.width = width;
			render_target_desc.height = height;
			render_target_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;
			render_target_desc.clear = rtv_clear_value;
			render_target_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			hdr_render_target = std::make_unique<Texture>(gfx, render_target_desc);
			hdr_render_target->CreateSRV();
			hdr_render_target->CreateRTV();

			prev_hdr_render_target = std::make_unique<Texture>(gfx, render_target_desc);
			prev_hdr_render_target->CreateSRV();
			prev_hdr_render_target->CreateRTV();

			sun_target = std::make_unique<Texture>(gfx, render_target_desc);
			sun_target->CreateSRV();
			sun_target->CreateRTV();
		}
		
		//depth stencil target
		{
			D3D12_CLEAR_VALUE clear_value{};
			clear_value.Format = DXGI_FORMAT_D32_FLOAT;
			clear_value.DepthStencil = { 1.0f, 0 };

			TextureDesc depth_desc{};
			depth_desc.width = width;
			depth_desc.height = height;
			depth_desc.format = DXGI_FORMAT_R32_TYPELESS;
			depth_desc.bind_flags = EBindFlag::DepthStencil | EBindFlag::ShaderResource;
			depth_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			depth_desc.clear = clear_value;

			depth_target = std::make_unique<Texture>(gfx, depth_desc);

			TextureViewDesc srv_desc{};
			srv_desc.new_format = DXGI_FORMAT_R32_FLOAT;
			depth_target->CreateSRV();
			TextureViewDesc dsv_desc{};
			dsv_desc.new_format = DXGI_FORMAT_D32_FLOAT;
			depth_target->CreateDSV();
		}

		//low dynamic range render target
		{
			rtv_clear_value.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			TextureDesc ldr_desc{};
			ldr_desc.width = width;
			ldr_desc.height = height;
			ldr_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
			ldr_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;
			ldr_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
			ldr_desc.clear = rtv_clear_value;

			ldr_render_target = std::make_unique<Texture>(gfx, ldr_desc);
			ldr_render_target->CreateSRV();
			ldr_render_target->CreateRTV();
		}

		//gbuffer
		{
			gbuffer.clear();

			rtv_clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			TextureDesc gbuffer_desc{};
			gbuffer_desc.width = width;
			gbuffer_desc.height = height;
			gbuffer_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			gbuffer_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;
			gbuffer_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			gbuffer_desc.clear = rtv_clear_value;

			for (uint32 i = 0; i < GBUFFER_SIZE; ++i)
			{
				gbuffer.emplace_back(std::make_unique<Texture>(gfx, gbuffer_desc));
				gbuffer.back()->CreateRTV();
				gbuffer.back()->CreateSRV();
			}
		}
		//ao
		{
			rtv_clear_value.Format = DXGI_FORMAT_R8_UNORM;
			TextureDesc ssao_target_desc{};
			ssao_target_desc.width = width;
			ssao_target_desc.height = height;
			ssao_target_desc.clear = rtv_clear_value;
			ssao_target_desc.format = DXGI_FORMAT_R8_UNORM;
			ssao_target_desc.initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; 
			ssao_target_desc.bind_flags = EBindFlag::RenderTarget | EBindFlag::ShaderResource;

			ao_texture = std::make_unique<Texture>(gfx, ssao_target_desc);
			ao_texture->CreateSRV();
			ao_texture->CreateRTV();
		}

		//blur
		{
			TextureDesc blur_desc{};
			blur_desc.width = width;
			blur_desc.height = height;
			blur_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
			blur_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			blur_desc.initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; 
			blur_intermediate_texture = std::make_unique<Texture>(gfx, blur_desc);

			blur_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			blur_final_texture = std::make_unique<Texture>(gfx, blur_desc);

			blur_intermediate_texture->CreateSRV();
			blur_final_texture->CreateSRV();

			blur_intermediate_texture->CreateUAV();
			blur_final_texture->CreateUAV();
		}

		//bloom
		{
			TextureDesc bloom_extract_desc{};
			bloom_extract_desc.width = width;
			bloom_extract_desc.height = height;
			bloom_extract_desc.mip_levels = 5;
			bloom_extract_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
			bloom_extract_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			bloom_extract_desc.initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; //t0 in CS

			bloom_extract_texture = std::make_unique<Texture>(gfx, bloom_extract_desc);
			bloom_extract_texture->CreateSRV();
			bloom_extract_texture->CreateUAV();
		}

		//postprocess
		{
			TextureDesc render_target_desc{};
			rtv_clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			render_target_desc.clear = rtv_clear_value;
			render_target_desc.width = width;
			render_target_desc.height = height;
			render_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			render_target_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
			render_target_desc.initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

			postprocess_textures[0] = std::make_unique<Texture>(gfx, render_target_desc);
			postprocess_textures[0]->CreateSRV();
			postprocess_textures[0]->CreateRTV();
			postprocess_textures[0]->CreateUAV();

			render_target_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postprocess_textures[1] = std::make_unique<Texture>(gfx, render_target_desc);
			postprocess_textures[1]->CreateSRV();
			postprocess_textures[1]->CreateRTV();
			postprocess_textures[1]->CreateUAV();
		}

		//tiled deferred
		{

			TextureDesc uav_target_desc{};
			uav_target_desc.width = width;
			uav_target_desc.height = height;
			uav_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			uav_target_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
			uav_target_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			uav_target = std::make_unique<Texture>(gfx, uav_target_desc);
			uav_target->CreateSRV();
			uav_target->CreateUAV();

			TextureDesc tiled_debug_desc{};
			tiled_debug_desc.width = width;
			tiled_debug_desc.height = height;
			tiled_debug_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tiled_debug_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::ShaderResource;
			tiled_debug_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			debug_tiled_texture = std::make_unique<Texture>(gfx, tiled_debug_desc);
			debug_tiled_texture->CreateSRV();
			debug_tiled_texture->CreateUAV();
		}

		//offscreen backbuffer
		{
			TextureDesc offscreen_ldr_target_desc{};
			rtv_clear_value.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			offscreen_ldr_target_desc.clear = rtv_clear_value;
			offscreen_ldr_target_desc.width = width;
			offscreen_ldr_target_desc.height = height;
			offscreen_ldr_target_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
			offscreen_ldr_target_desc.bind_flags = EBindFlag::ShaderResource | EBindFlag::RenderTarget;
			offscreen_ldr_target_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			offscreen_ldr_target = std::make_unique<Texture>(gfx, offscreen_ldr_target_desc);

			offscreen_ldr_target->CreateSRV();
			offscreen_ldr_target->CreateRTV();
		}

		//bokeh
		{
			bokeh = std::make_unique<Buffer>(gfx, StructuredBufferDesc<Bokeh>(width * height));
			bokeh->CreateSRV();
			bokeh->CreateUAV(bokeh_counter.GetNative());
		}
		
		//velocity buffer
		{
			TextureDesc velocity_buffer_desc{};
			rtv_clear_value.Format = DXGI_FORMAT_R16G16_FLOAT;
			velocity_buffer_desc.clear = rtv_clear_value;
			velocity_buffer_desc.width = width;
			velocity_buffer_desc.height = height;
			velocity_buffer_desc.format = DXGI_FORMAT_R16G16_FLOAT;
			velocity_buffer_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			velocity_buffer_desc.bind_flags = EBindFlag::ShaderResource | EBindFlag::RenderTarget;

			velocity_buffer = std::make_unique<Texture>(gfx, velocity_buffer_desc);
			velocity_buffer->CreateSRV();
			velocity_buffer->CreateRTV();
		}

	}
	void Renderer::CreateOtherResources()
	{
		//shadow maps
		{
			//shadow map
			{
				D3D12_CLEAR_VALUE clear_value{};
				clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				clear_value.DepthStencil = { 1.0f, 0 };
				TextureDesc depth_map_desc{};
				depth_map_desc.width = SHADOW_MAP_SIZE;
				depth_map_desc.height = SHADOW_MAP_SIZE;
				depth_map_desc.format = DXGI_FORMAT_R32_TYPELESS;
				depth_map_desc.bind_flags = EBindFlag::DepthStencil | EBindFlag::ShaderResource;
				depth_map_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				depth_map_desc.clear = clear_value;
				shadow_depth_map = std::make_unique<Texture>(gfx, depth_map_desc);

				TextureViewDesc srv_desc{};
				srv_desc.new_format = DXGI_FORMAT_R32_FLOAT;
				shadow_depth_map->CreateSRV(&srv_desc);

				TextureViewDesc dsv_desc{};
				dsv_desc.new_format = DXGI_FORMAT_D32_FLOAT;
				shadow_depth_map->CreateDSV(&dsv_desc);
			}

			//shadow cubemap
			{
				TextureDesc cubemap_desc{};
				cubemap_desc.width = SHADOW_CUBE_SIZE;
				cubemap_desc.height = SHADOW_CUBE_SIZE;
				cubemap_desc.misc_flags = EResourceMiscFlag::TextureCube;
				cubemap_desc.array_size = 6;
				cubemap_desc.format = DXGI_FORMAT_R32_TYPELESS;
				cubemap_desc.clear = D3D12_CLEAR_VALUE{ .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {1.0f, 0}};
				cubemap_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				cubemap_desc.bind_flags = EBindFlag::ShaderResource | EBindFlag::DepthStencil;

				shadow_depth_cubemap = std::make_unique<Texture>(gfx, cubemap_desc);

				TextureViewDesc cube_srv_desc{};
				cube_srv_desc.new_format = DXGI_FORMAT_R32_FLOAT;
				cube_srv_desc.first_mip = 0;
				cube_srv_desc.mip_count = 1;
				shadow_depth_cubemap->CreateSRV(&cube_srv_desc);

				TextureViewDesc cube_dsv_desc{};
				cube_dsv_desc.new_format = DXGI_FORMAT_D32_FLOAT;
				cube_dsv_desc.first_mip = 0;
				cube_dsv_desc.slice_count = 1;
				for (size_t i = 0; i < 6; ++i)
				{
					cube_dsv_desc.first_slice = i;
					size_t j = shadow_depth_cubemap->CreateDSV(&cube_dsv_desc);
					ADRIA_ASSERT(j == i);
				}
			}

			//shadow cascades
			{
				TextureDesc depth_cascade_maps_desc{};
				depth_cascade_maps_desc.width = SHADOW_CASCADE_MAP_SIZE;
				depth_cascade_maps_desc.height = SHADOW_CASCADE_MAP_SIZE;
				depth_cascade_maps_desc.misc_flags = EResourceMiscFlag::None;
				depth_cascade_maps_desc.array_size = CASCADE_COUNT;
				depth_cascade_maps_desc.format = DXGI_FORMAT_R32_TYPELESS;
				depth_cascade_maps_desc.clear = D3D12_CLEAR_VALUE{ .Format = DXGI_FORMAT_D32_FLOAT, .DepthStencil = {1.0f, 0} };
				depth_cascade_maps_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				depth_cascade_maps_desc.bind_flags = EBindFlag::ShaderResource | EBindFlag::DepthStencil;

				shadow_depth_cascades = std::make_unique<Texture>(gfx, depth_cascade_maps_desc);

				TextureViewDesc srv_desc{};
				srv_desc.new_format = DXGI_FORMAT_R32_FLOAT;
				srv_desc.mip_count = 1;
				shadow_depth_cascades->CreateSRV(&srv_desc);

				TextureViewDesc dsv_desc{};
				dsv_desc.new_format = DXGI_FORMAT_D32_FLOAT;
				dsv_desc.first_mip = 0;
				dsv_desc.slice_count = 1;
				for (size_t i = 0; i < CASCADE_COUNT; ++i)
				{
					dsv_desc.first_slice = D3D12CalcSubresource(0, i, 0, 1, 1);
					shadow_depth_cascades->CreateDSV(&dsv_desc);
				}
			}
		}

		//ao
		{
			TextureDesc noise_desc{};
			noise_desc.width = SSAO_NOISE_DIM;
			noise_desc.height = SSAO_NOISE_DIM;
			noise_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			noise_desc.initial_state = D3D12_RESOURCE_STATE_COPY_DEST;
			noise_desc.bind_flags = EBindFlag::ShaderResource;

			ssao_random_texture = std::make_unique<Texture>(gfx, noise_desc);
			hbao_random_texture = std::make_unique<Texture>(gfx, noise_desc);

			ssao_random_texture->CreateSRV();
			hbao_random_texture->CreateSRV();

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
			TextureDesc uav_desc{};
			uav_desc.width = RESOLUTION;
			uav_desc.height = RESOLUTION;
			uav_desc.format = DXGI_FORMAT_R32_FLOAT;
			uav_desc.bind_flags = EBindFlag::ShaderResource | EBindFlag::UnorderedAccess;
			uav_desc.initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			ocean_initial_spectrum = std::make_unique<Texture>(gfx, uav_desc);
			ocean_initial_spectrum->CreateSRV();
			ocean_initial_spectrum->CreateUAV();

			uav_desc.initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			ping_pong_phase_textures[pong_phase] = std::make_unique<Texture>(gfx, uav_desc);
			ping_pong_phase_textures[pong_phase]->CreateSRV();
			ping_pong_phase_textures[pong_phase]->CreateUAV();
			uav_desc.initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			ping_pong_phase_textures[!pong_phase] = std::make_unique<Texture>(gfx, uav_desc);
			ping_pong_phase_textures[!pong_phase]->CreateSRV();
			ping_pong_phase_textures[!pong_phase]->CreateUAV();

			uav_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;

			uav_desc.initial_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			ping_pong_spectrum_textures[pong_spectrum] = std::make_unique<Texture>(gfx, uav_desc);
			ping_pong_spectrum_textures[pong_spectrum]->CreateSRV();
			ping_pong_spectrum_textures[pong_spectrum]->CreateUAV();

			uav_desc.initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			ping_pong_spectrum_textures[!pong_spectrum] = std::make_unique<Texture>(gfx, uav_desc);
			ping_pong_spectrum_textures[!pong_spectrum]->CreateSRV();
			ping_pong_spectrum_textures[!pong_spectrum]->CreateUAV();

			uav_desc.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			ocean_normal_map = std::make_unique<Texture>(gfx, uav_desc);
			ocean_normal_map->CreateSRV();
			ocean_normal_map->CreateUAV();
		}

		//clustered deferred
		{
			light_counter.CreateSRV();
			light_list.CreateSRV();
			clusters.CreateSRV();
			light_grid.CreateSRV();

			light_counter.CreateUAV();
			light_list.CreateUAV();
			clusters.CreateUAV();
			light_grid.CreateUAV();
		}

		picker.CreateView();
	}
	void Renderer::CreateRenderPasses(uint32 width, uint32 height)
	{
		static D3D12_CLEAR_VALUE black = { .Format = DXGI_FORMAT_UNKNOWN, .Color = {0.0f, 0.0f, 0.0f, 1.0f}, };

		//gbuffer render pass
		{
			RenderPassDesc render_pass_desc{};

			RtvAttachmentDesc gbuffer_normal_attachment{};
			gbuffer_normal_attachment.cpu_handle = gbuffer[0]->RTV();
			gbuffer_normal_attachment.clear_value = black;
			gbuffer_normal_attachment.clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			gbuffer_normal_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			gbuffer_normal_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(gbuffer_normal_attachment);

			RtvAttachmentDesc gbuffer_albedo_attachment{};
			gbuffer_albedo_attachment.cpu_handle = gbuffer[1]->RTV();
			gbuffer_albedo_attachment.clear_value = black;
			gbuffer_normal_attachment.clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			gbuffer_albedo_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			gbuffer_albedo_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(gbuffer_albedo_attachment);

			RtvAttachmentDesc gbuffer_emissive_attachment{};
			gbuffer_emissive_attachment.cpu_handle = gbuffer[2]->RTV();
			gbuffer_emissive_attachment.clear_value = black;
			gbuffer_emissive_attachment.clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			gbuffer_emissive_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			gbuffer_emissive_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(gbuffer_emissive_attachment);

			DsvAttachmentDesc dsv_attachment_desc{};
			dsv_attachment_desc.cpu_handle = depth_target->DSV();
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
			RenderPassDesc render_pass_desc{};

			RtvAttachmentDesc decal_albedo_attachment{};
			decal_albedo_attachment.cpu_handle = gbuffer[1]->RTV();
			decal_albedo_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			decal_albedo_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(decal_albedo_attachment);

			RtvAttachmentDesc decal_normal_attachment{};
			decal_normal_attachment.cpu_handle = gbuffer[0]->RTV();
			decal_normal_attachment.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			decal_normal_attachment.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(decal_normal_attachment);

			render_pass_desc.width = width;
			render_pass_desc.height = height;
			decal_pass = RenderPass(render_pass_desc);
		}

		//ambient render pass 
		{
			RenderPassDesc render_pass_desc{};
			RtvAttachmentDesc rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target->RTV();
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
			RenderPassDesc render_pass_desc{};
			RtvAttachmentDesc rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target->RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);

			render_pass_desc.width = width;
			render_pass_desc.height = height;

			lighting_render_pass = RenderPass(render_pass_desc);
		}

		//forward render pass
		{
			RenderPassDesc render_pass_desc{};
			RtvAttachmentDesc rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target->RTV();
			rtv_attachment_desc.clear_value = black;
			rtv_attachment_desc.clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);

			DsvAttachmentDesc dsv_attachment_desc{};
			dsv_attachment_desc.cpu_handle = depth_target->DSV();
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
			RenderPassDesc render_pass_desc{};
			RtvAttachmentDesc rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = ldr_render_target->RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);
			render_pass_desc.width = width;
			render_pass_desc.height = height;
			fxaa_render_pass = RenderPass(render_pass_desc);
		}

		//shadow map pass
		{
			RenderPassDesc render_pass_desc{};
			DsvAttachmentDesc dsv_attachment_desc{};
			dsv_attachment_desc.cpu_handle = shadow_depth_map->DSV();
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
				RenderPassDesc render_pass_desc{};
				DsvAttachmentDesc dsv_attachment_desc{};
				dsv_attachment_desc.cpu_handle = shadow_depth_cubemap->DSV(i);
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
				RenderPassDesc render_pass_desc{};
				DsvAttachmentDesc dsv_attachment_desc{};
				dsv_attachment_desc.cpu_handle = shadow_depth_cascades->DSV(i);
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
			RenderPassDesc render_pass_desc{};
			RtvAttachmentDesc ao_attachment_desc{};
			ao_attachment_desc.cpu_handle = ao_texture->RTV();
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
			RenderPassDesc render_pass_desc{};
			RtvAttachmentDesc rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = hdr_render_target->RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);

			render_pass_desc.width = width;
			render_pass_desc.height = height;
			particle_pass = RenderPass(render_pass_desc);
		}

		//postprocess passes
		{
			RenderPassDesc ping_render_pass_desc{};
			RtvAttachmentDesc ping_attachment_desc{};
			ping_attachment_desc.cpu_handle = postprocess_textures[0]->RTV();
			ping_attachment_desc.clear_value = black;
			ping_attachment_desc.clear_value.Format = postprocess_textures[0]->GetDesc().format;
			ping_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			ping_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			ping_render_pass_desc.rtv_attachments.push_back(ping_attachment_desc);

			ping_render_pass_desc.width = width;
			ping_render_pass_desc.height = height;

			postprocess_passes[0] = RenderPass(ping_render_pass_desc);

			
			RenderPassDesc pong_render_pass_desc{};
			RtvAttachmentDesc pong_attachment_desc{};
			pong_attachment_desc.cpu_handle = postprocess_textures[1]->RTV();
			pong_attachment_desc.clear_value = black;
			pong_attachment_desc.clear_value.Format = postprocess_textures[1]->GetDesc().format;
			pong_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			pong_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			pong_render_pass_desc.rtv_attachments.push_back(pong_attachment_desc);

			pong_render_pass_desc.width = width;
			pong_render_pass_desc.height = height;

			postprocess_passes[1] = RenderPass(pong_render_pass_desc);

		}

		//offscreen resolve pass
		{
			RenderPassDesc render_pass_desc{};
			RtvAttachmentDesc rtv_attachment_desc{};
			rtv_attachment_desc.cpu_handle = offscreen_ldr_target->RTV();
			rtv_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			rtv_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(rtv_attachment_desc);
			render_pass_desc.width = width;
			render_pass_desc.height = height;
			offscreen_resolve_pass = RenderPass(render_pass_desc);
		}

		//velocity buffer pass
		{
			RenderPassDesc render_pass_desc{};

			RtvAttachmentDesc velocity_buffer_attachment{};
			velocity_buffer_attachment.cpu_handle = velocity_buffer->RTV();
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
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		
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

		device->CreateShaderResourceView(env_texture.Get(), &env_srv_desc, ibl_heap->GetHandle(ENV_TEXTURE_SLOT));

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
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), unfiltered_env_descriptor,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

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
					descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
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

		device->CreateShaderResourceView(irmap_texture.Get(), &irmap_srv_desc, ibl_heap->GetHandle(IRMAP_TEXTURE_SLOT));


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
				descriptor_allocator->GetHandle(descriptor_index));

			auto irmap_barrier = CD3DX12_RESOURCE_BARRIER::Transition(irmap_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			cmd_list->ResourceBarrier(1, &irmap_barrier);
			cmd_list->SetPipelineState(pipeline_state.Get());
			cmd_list->SetComputeRootSignature(root_signature.Get());
			ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
			cmd_list->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);

			device->CopyDescriptorsSimple(1,
				descriptor_allocator->GetHandle(descriptor_index + 1), ibl_heap->GetHandle(ENV_TEXTURE_SLOT),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index + 1));
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
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

		device->CreateShaderResourceView(brdf_lut_texture.Get(), &brdf_srv_desc, ibl_heap->GetHandle(BRDF_LUT_TEXTURE_SLOT));

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
				descriptor_allocator->GetHandle(descriptor_index));

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

			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
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

			BufferDesc vb_desc{};
			vb_desc.bind_flags = EBindFlag::VertexBuffer;
			vb_desc.size = sizeof(cube_vertices);
			vb_desc.stride = sizeof(SimpleVertex);
			cube_vb = std::make_shared<Buffer>(gfx, vb_desc, cube_vertices);

			BufferDesc ib_desc{};
			ib_desc.bind_flags = EBindFlag::IndexBuffer;
			ib_desc.format = DXGI_FORMAT_R16_UINT;
			ib_desc.stride = sizeof(uint16);
			ib_desc.size = sizeof(cube_indices);
			cube_ib = std::make_shared<Buffer>(gfx, ib_desc, cube_indices);
		}

		//ao textures
		ID3D12Resource* ssao_upload_texture = nullptr;
		{
			const uint64 upload_buffer_size = GetRequiredIntermediateSize(ssao_random_texture->GetNative(), 0, 1);

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

			UpdateSubresources(gfx->GetDefaultCommandList(), ssao_random_texture->GetNative(), ssao_upload_texture, 0, 0, 1, &data);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(ssao_random_texture->GetNative(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			gfx->GetDefaultCommandList()->ResourceBarrier(1, &barrier);
		}

		ID3D12Resource* hbao_upload_texture = nullptr;
		{
			const uint64 upload_buffer_size = GetRequiredIntermediateSize(hbao_random_texture->GetNative(), 0, 1);

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

			UpdateSubresources(gfx->GetDefaultCommandList(), hbao_random_texture->GetNative(), hbao_upload_texture, 0, 0, 1, &data);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(hbao_random_texture->GetNative(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			gfx->GetDefaultCommandList()->ResourceBarrier(1, &barrier);
		}

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

			BufferDesc buffer_desc{};
			buffer_desc.size = 4 * sizeof(uint32);
			buffer_desc.bind_flags = EBindFlag::None;
			buffer_desc.misc_flags = EResourceMiscFlag::IndirectArgs;
			buffer_desc.heap_type = EHeapType::Default;

			uint32 init_data[] = { 0,1,0,0 };
			bokeh_indirect_draw_buffer = std::make_unique<Buffer>(gfx, buffer_desc, init_data);

			hex_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Hex.dds");
			oct_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Oct.dds");
			circle_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Circle.dds");
			cross_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Cross.dds");

			D3D12_INDIRECT_ARGUMENT_DESC args[1];
			args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

			D3D12_COMMAND_SIGNATURE_DESC command_signature_desc{};
			command_signature_desc.NumArgumentDescs = 1;
			command_signature_desc.pArgumentDescs = args;
			command_signature_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&bokeh_command_signature)));

			BufferDesc reset_buffer_desc{};
			reset_buffer_desc.size = sizeof(uint32);
			reset_buffer_desc.heap_type = EHeapType::Upload;
			uint32 initial_data[] = { 0 };
			counter_reset_buffer = std::make_unique<Buffer>(gfx, reset_buffer_desc, initial_data);
		}

		//ocean ping phase initial
		ID3D12Resource* ping_phase_upload_buffer = nullptr;
		{
			const uint64 upload_buffer_size = GetRequiredIntermediateSize(ping_pong_phase_textures[pong_phase]->GetNative(), 0, 1);
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
			barrier.Transition.pResource = ping_pong_phase_textures[pong_phase]->GetNative();
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			ID3D12GraphicsCommandList* cmd_list = gfx->GetDefaultCommandList();
			cmd_list->ResourceBarrier(1, &barrier);
			UpdateSubresources(cmd_list, ping_pong_phase_textures[pong_phase]->GetNative(), ping_phase_upload_buffer, 0, 0, 1, &data);
			std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
			cmd_list->ResourceBarrier(1, &barrier);
		}

		gfx->AddToReleaseQueue(ssao_upload_texture);
		gfx->AddToReleaseQueue(hbao_upload_texture);
		gfx->AddToReleaseQueue(ping_phase_upload_buffer);

		particle_renderer.UploadData();
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
		picker.Pick(cmd_list, depth_target->SRV(), gbuffer[0]->SRV(), frame_cbuffer.View(backbuffer_index).BufferLocation);
	}
	void Renderer::PassGBuffer(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "GBuffer Pass");
		SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::GBufferPass, profiler_settings.profile_gbuffer_pass);

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();
		auto gbuffer_view = reg.view<Mesh, Transform, Material, Deferred, Visibility>();

		ResourceBarrierBatch gbuffer_barriers{};
		for (auto& texture : gbuffer)
			gbuffer_barriers.AddTransition(texture->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		gbuffer_barriers.Submit(cmd_list);

		gbuffer_render_pass.Begin(cmd_list);

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::GbufferPBR));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::GbufferPBR));
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetFirstHandle());

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
			material_cbuf_data.albedo_idx = material.albedo_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.albedo_texture) : 0;
			material_cbuf_data.normal_idx = material.normal_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.normal_texture) : 0;
			material_cbuf_data.metallic_roughness_idx = material.metallic_roughness_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.metallic_roughness_texture) : 0;
			material_cbuf_data.emissive_idx = material.emissive_texture != INVALID_TEXTURE_HANDLE ? static_cast<int32>(material.emissive_texture) : 0;

			DynamicAllocation material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			material_allocation.Update(material_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

			mesh.Draw(cmd_list);
		}
		gbuffer_render_pass.End(cmd_list);

		if (reg.size<Decal>() > 0)
		{
			gbuffer_barriers.Clear();
			gbuffer_barriers.AddTransition(gbuffer[2]->GetNative(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
		SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::DecalPass, profiler_settings.profile_decal_pass);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Decal Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

		struct DecalCBuffer
		{
			int32 decal_type;
		};
		DecalCBuffer decal_cbuf_data{};
		ObjectCBuffer object_cbuf_data{};

		decal_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Decals));
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			auto decal_view = reg.view<Decal>();

			auto decal_pass_lambda = [&](bool modify_normals)
			{
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(modify_normals ? EPipelineStateObject::Decals_ModifyNormals : EPipelineStateObject::Decals));
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
					texture_handles.push_back(depth_target->SRV());
					src_range_sizes.assign(texture_handles.size(), 1u);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(texture_handles.size());
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)texture_handles.size() };
					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, (uint32)texture_handles.size(), texture_handles.data(), src_range_sizes.data(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetGraphicsRootDescriptorTable(3, dst_descriptor);

					cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					BindVertexBuffer(cmd_list, cube_vb.get());
					BindIndexBuffer(cmd_list, cube_ib.get());
					cmd_list->DrawIndexedInstanced(cube_ib->GetCount(), 1, 0, 0, 0);
				}
			};
			decal_pass_lambda(false);
			decal_pass_lambda(true);
		}
		decal_pass.End(cmd_list);

		ResourceBarrierBatch decal_barriers{};
		decal_barriers.AddTransition(gbuffer[0]->GetNative(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		decal_barriers.AddTransition(gbuffer[1]->GetNative(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		decal_barriers.Submit(cmd_list);
	}

	void Renderer::PassSSAO(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.ambient_occlusion == EAmbientOcclusion::SSAO);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "SSAO Pass");

		ResourceBarrierBatch ssao_barrier{};
		ssao_barrier.AddTransition(ao_texture->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		ssao_barrier.Submit(cmd_list);

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		ssao_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::AO));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::SSAO));

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
			auto descriptor = descriptor_allocator->GetHandle(descriptor_index);
			D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0]->SRV(), depth_target->SRV(), ssao_random_texture->SRV() };
			D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor };
			UINT src_range_sizes[] = { 1, 1, 1 };
			UINT dst_range_sizes[] = { 3 };
			device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetGraphicsRootDescriptorTable(2, descriptor);

			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmd_list->DrawInstanced(4, 1, 0, 0);
		}
		ssao_render_pass.End(cmd_list);

		ssao_barrier.ReverseTransitions();
		ssao_barrier.Submit(cmd_list);

		BlurTexture(cmd_list, *ao_texture);
	}
	void Renderer::PassHBAO(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.ambient_occlusion == EAmbientOcclusion::HBAO);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "HBAO Pass");

		ResourceBarrierBatch hbao_barrier{};
		hbao_barrier.AddTransition(ao_texture->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		hbao_barrier.Submit(cmd_list);

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		hbao_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::AO));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::HBAO));

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
			auto descriptor = descriptor_allocator->GetHandle(descriptor_index);

			D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0]->SRV(), depth_target->SRV(), hbao_random_texture->SRV() };
			D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor };
			UINT src_range_sizes[] = { 1, 1, 1 };
			UINT dst_range_sizes[] = { 3 };
			device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetGraphicsRootDescriptorTable(2, descriptor);

			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmd_list->DrawInstanced(4, 1, 0, 0);
		}
		hbao_render_pass.End(cmd_list);

		hbao_barrier.ReverseTransitions();
		hbao_barrier.Submit(cmd_list);

		BlurTexture(cmd_list, *ao_texture);
	}
	void Renderer::PassRTAO(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.ambient_occlusion == EAmbientOcclusion::RTAO);
		SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::RT_AmbientOcclusion, profiler_settings.profile_rtao);
		ray_tracer.RayTraceAmbientOcclusion(cmd_list, *depth_target, *gbuffer[0], frame_cbuffer.View(backbuffer_index).BufferLocation);
		BlurTexture(cmd_list, ray_tracer.GetRayTracingAmbientOcclusionTexture());
	}
	void Renderer::PassAmbient(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Ambient Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		if (!ibl_textures_generated) settings.ibl = false;

		ambient_render_pass.Begin(cmd_list);

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::AmbientPBR));
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

		bool has_ao = settings.ambient_occlusion != EAmbientOcclusion::None;
		if (has_ao && settings.ibl) cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::AmbientPBR_AO_IBL));
		else if (has_ao && !settings.ibl) cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::AmbientPBR_AO));
		else if (!has_ao && settings.ibl) cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::AmbientPBR_IBL));
		else cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::AmbientPBR));

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = {gbuffer[0]->SRV(), gbuffer[1]->SRV(), gbuffer[2]->SRV(), depth_target->SRV()};
		uint32 src_range_sizes[] = {1,1,1,1};
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
		auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
		uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
		device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, dst_descriptor);


		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { null_srv_heap->GetHandle(TEXTURE2D_SLOT), 
		null_srv_heap->GetHandle(TEXTURECUBE_SLOT), null_srv_heap->GetHandle(TEXTURECUBE_SLOT), null_srv_heap->GetHandle(TEXTURE2D_SLOT) };
		uint32 src_range_sizes2[] = { 1,1,1,1 };
		if (has_ao) cpu_handles2[0] = blur_final_texture->SRV(); //contains blurred ssao
		if (settings.ibl)
		{
			cpu_handles2[1] = ibl_heap->GetHandle(ENV_TEXTURE_SLOT);
			cpu_handles2[2] = ibl_heap->GetHandle(IRMAP_TEXTURE_SLOT);
			cpu_handles2[3] = ibl_heap->GetHandle(BRDF_LUT_TEXTURE_SLOT);
		}

		descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles2));
		dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
		uint32 dst_range_sizes2[] = { (uint32)ARRAYSIZE(cpu_handles2) };
		device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes2, ARRAYSIZE(cpu_handles2), cpu_handles2, src_range_sizes2,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

		ambient_render_pass.End(cmd_list);
	}
	void Renderer::PassDeferredLighting(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Deferred Lighting Pass");
		SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::DeferredPass, profiler_settings.profile_deferred_pass);

		ID3D12Device* device = gfx->GetDevice();
		auto upload_buffer = gfx->GetDynamicAllocator();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

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

			if (light_data.ray_traced_shadows)
			{
				SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::RT_Shadows, profiler_settings.profile_rts);

				D3D12_RESOURCE_BARRIER pre_rts_barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				};
				cmd_list->ResourceBarrier(ARRAYSIZE(pre_rts_barriers), pre_rts_barriers);

				ray_tracer.RayTraceShadows(cmd_list, *depth_target, frame_cbuffer.View(backbuffer_index).BufferLocation,
					light_allocation.gpu_address, light_data.soft_rts);

				D3D12_RESOURCE_BARRIER post_rts_barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				};
				cmd_list->ResourceBarrier(ARRAYSIZE(post_rts_barriers), post_rts_barriers);
			}
			else if (light_data.casts_shadows)
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
				cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::LightingPBR));
				cmd_list->SetPipelineState(light_data.ray_traced_shadows ? 
					RootSigPSOManager::GetPipelineState(EPipelineStateObject::LightingPBR_RayTracedShadows) :
					RootSigPSOManager::GetPipelineState(EPipelineStateObject::LightingPBR));

				cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

				cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);

				cmd_list->SetGraphicsRootConstantBufferView(2, shadow_allocation.gpu_address);

				//t0,t1,t2 - gbuffer and depth
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0]->SRV(), gbuffer[1]->SRV(), depth_target->SRV() };
					uint32 src_range_sizes[] = { 1,1,1 };

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetGraphicsRootDescriptorTable(3, dst_descriptor);
				}

				//t4,t5,t6 - shadow maps
				{
					D3D12_CPU_DESCRIPTOR_HANDLE shadow_cpu_handles[] = { null_srv_heap->GetHandle(TEXTURE2D_SLOT),
						null_srv_heap->GetHandle(TEXTURECUBE_SLOT), null_srv_heap->GetHandle(TEXTURE2DARRAY_SLOT), 
						null_srv_heap->GetHandle(TEXTURE2D_SLOT) };
					uint32 src_range_sizes[] = { 1,1,1,1 };

					if (light_data.ray_traced_shadows)
					{
						shadow_cpu_handles[3] = ray_tracer.GetRayTracingShadowsTexture().SRV();
					}
					else if (light_data.casts_shadows)
					{
						switch (light_data.type)
						{
						case ELightType::Directional:
							if (light_data.use_cascades) shadow_cpu_handles[2] = shadow_depth_cascades->SRV();
							else shadow_cpu_handles[0] = shadow_depth_map->SRV();
							break;
						case ELightType::Spot:
							shadow_cpu_handles[0] = shadow_depth_map->SRV();
							break;
						case ELightType::Point:
							shadow_cpu_handles[1] = shadow_depth_cubemap->SRV();
							break;
						default:
							ADRIA_ASSERT(false);
						}
					}

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(shadow_cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(shadow_cpu_handles) };
					device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(shadow_cpu_handles), shadow_cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetGraphicsRootDescriptorTable(4, dst_descriptor);
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
		auto upload_buffer = gfx->GetDynamicAllocator();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

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
		tiled_barriers.AddTransition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tiled_barriers.AddTransition(gbuffer[0]->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tiled_barriers.AddTransition(gbuffer[1]->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		tiled_barriers.AddTransition(uav_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		tiled_barriers.AddTransition(debug_tiled_texture->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		tiled_barriers.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::TiledLighting));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::TiledLighting));

		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(1, compute_cbuffer.View(backbuffer_index).BufferLocation);

		//t0,t1,t2 - gbuffer and depth
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0]->SRV(), gbuffer[1]->SRV(), depth_target->SRV() };
			uint32 src_range_sizes[] = { 1,1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(2, dst_descriptor);
		}

		D3D12_GPU_DESCRIPTOR_HANDLE uav_target_for_clear{};
		D3D12_GPU_DESCRIPTOR_HANDLE uav_debug_for_clear{};
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { uav_target->UAV(), debug_tiled_texture->UAV() };
			uint32 src_range_sizes[] = { 1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(3, dst_descriptor);
			uav_target_for_clear = descriptor_allocator->GetHandle(descriptor_index);
			uav_debug_for_clear = descriptor_allocator->GetHandle(descriptor_index + 1);
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

			auto handle = descriptor_allocator->GetHandle(i);
			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, handle);
			cmd_list->SetComputeRootDescriptorTable(4, handle);
		}

		float32 black[4] = { 0.0f,0.0f,0.0f,0.0f };

		cmd_list->ClearUnorderedAccessViewFloat(uav_target_for_clear, uav_target->UAV(), uav_target->GetNative(),
			black, 0, nullptr);
		cmd_list->ClearUnorderedAccessViewFloat(uav_debug_for_clear, debug_tiled_texture->UAV(), debug_tiled_texture->GetNative(),
			black, 0, nullptr);

		cmd_list->Dispatch((uint32)std::ceil(width * 1.0f / 16), (uint32)(height * 1.0f / 16), 1);

		tiled_barriers.ReverseTransitions();
		tiled_barriers.Submit(cmd_list);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = hdr_render_target->RTV();
		cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		if (settings.visualize_tiled) AddTextures(cmd_list, *uav_target, *debug_tiled_texture, EBlendMode::AlphaBlend);
		else CopyTexture(cmd_list, *uav_target, EBlendMode::AdditiveBlend);
	}
	void Renderer::PassDeferredClusteredLighting(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.use_clustered_deferred);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Deferred Clustered Lighting Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto upload_buffer = gfx->GetDynamicAllocator();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

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
			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ClusterBuilding));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ClusterBuilding));
			cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			device->CopyDescriptorsSimple(1, dst_descriptor, clusters.UAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, dst_descriptor);

			cmd_list->Dispatch(CLUSTER_SIZE_X, CLUSTER_SIZE_Y, CLUSTER_SIZE_Z);
			recreate_clusters = false;
		}

		//cluster building
		{
			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ClusterCulling));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ClusterCulling));
		
			OffsetType i = descriptor_allocator->AllocateRange(2);
			auto dst_descriptor = descriptor_allocator->GetHandle(i);
			device->CopyDescriptorsSimple(1, dst_descriptor, clusters.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
			desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;
			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, descriptor_allocator->GetHandle(i + 1));
		
			cmd_list->SetComputeRootDescriptorTable(0, dst_descriptor);
		
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { light_counter.UAV(), light_list.UAV(), light_grid.UAV() };
			uint32 src_range_sizes[] = { 1,1,1 };
			i = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			dst_descriptor = descriptor_allocator->GetHandle(i);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
			cmd_list->SetComputeRootDescriptorTable(1, dst_descriptor);
			cmd_list->Dispatch(CLUSTER_SIZE_X / 16, CLUSTER_SIZE_Y / 16, CLUSTER_SIZE_Z / 1);
		}

		//clustered lighting
		lighting_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ClusteredLightingPBR));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ClusteredLightingPBR));
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

			//gbuffer
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0]->SRV(), gbuffer[1]->SRV(), depth_target->SRV() };
			uint32 src_range_sizes[] = { 1,1,1 };
			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
			auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };
			device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetGraphicsRootDescriptorTable(1, dst_descriptor);

			//light stuff
			descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles) + 1);
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { light_list.SRV(), light_grid.SRV() };
			uint32 src_range_sizes2[] = { 1,1 };

			dst_descriptor = descriptor_allocator->GetHandle(descriptor_index + 1);
			uint32 dst_range_sizes2[] = { (uint32)ARRAYSIZE(cpu_handles2) };
			device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes2, ARRAYSIZE(cpu_handles2), cpu_handles2, src_range_sizes2,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = static_cast<uint32>(dynamic_alloc.size / desc.Buffer.StructureByteStride);
			desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;
			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, dst_descriptor);

			cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);
			cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			cmd_list->DrawInstanced(4, 1, 0, 0);
		}
		lighting_render_pass.End(cmd_list);

	}
	void Renderer::PassForward(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Forward Pass");
		SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::ForwardPass, profiler_settings.profile_forward_pass);

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
		SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::Postprocessing, profiler_settings.profile_postprocessing);

		PassVelocityBuffer(cmd_list);

		auto lights = reg.view<Light>();
		postprocess_passes[postprocess_index].Begin(cmd_list); //set ping as rt
		CopyTexture(cmd_list, *hdr_render_target);
		for (entity light : lights)
		{
			auto const& light_data = lights.get(light);
			if (!light_data.active || !light_data.lens_flare) continue;

			PassLensFlare(cmd_list, light_data);
		}
		postprocess_passes[postprocess_index].End(cmd_list); //now we have copy of scene in ping

		ResourceBarrierBatch postprocess_barriers{};
		postprocess_barriers.AddTransition(postprocess_textures[postprocess_index]->GetNative(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		postprocess_barriers.AddTransition(postprocess_textures[!postprocess_index]->GetNative(),
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
			cloud_barriers.AddTransition(postprocess_textures[!postprocess_index]->GetNative(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cloud_barriers.Submit(cmd_list);

			BlurTexture(cmd_list, *postprocess_textures[!postprocess_index]);

			cloud_barriers.ReverseTransitions();
			cloud_barriers.Submit(cmd_list);

			postprocess_passes[postprocess_index].Begin(cmd_list);
			CopyTexture(cmd_list, *blur_final_texture, EBlendMode::AlphaBlend);
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

		if (settings.reflections == EReflections::SSR)
		{
			postprocess_passes[postprocess_index].Begin(cmd_list);

			PassSSR(cmd_list);

			postprocess_passes[postprocess_index].End(cmd_list);
			
			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}
		else if (settings.reflections == EReflections::RTR)
		{
			SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::RT_Reflections, profiler_settings.profile_rtr);

			D3D12_CPU_DESCRIPTOR_HANDLE skybox_handle = null_srv_heap->GetHandle(TEXTURECUBE_SLOT);
			if (settings.sky_type == ESkyType::Skybox)
			{
				auto skybox_entities = reg.view<Skybox>();
				for (auto e : skybox_entities)
				{
					Skybox skybox = skybox_entities.get(e);
					if (skybox.active && skybox.used_in_rt)
					{
						skybox_handle = texture_manager.CpuDescriptorHandle(skybox.cubemap_texture);
						break;
					}
				}
			}

			ray_tracer.RayTraceReflections(cmd_list, *depth_target, frame_cbuffer.View(backbuffer_index).BufferLocation, skybox_handle);

			postprocess_passes[postprocess_index].Begin(cmd_list);

			AddTextures(cmd_list, *postprocess_textures[!postprocess_index], ray_tracer.GetRayTracingReflectionsTexture(), EBlendMode::None);

			postprocess_passes[postprocess_index].End(cmd_list);
			
			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}

		if (settings.dof)
		{
			ResourceBarrierBatch barrier{};

			barrier.AddTransition(postprocess_textures[!postprocess_index]->GetNative(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			barrier.Submit(cmd_list);

			BlurTexture(cmd_list, *postprocess_textures[!postprocess_index]);
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
					AddTextures(cmd_list, *postprocess_textures[!postprocess_index], *sun_target);
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
			taa_barrier.AddTransition(prev_hdr_render_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET);

			taa_barrier.Submit(cmd_list);
			auto rtv = prev_hdr_render_target->RTV();
			cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

			CopyTexture(cmd_list, *postprocess_textures[!postprocess_index]);
			
			cmd_list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
			taa_barrier.ReverseTransitions();
			taa_barrier.Submit(cmd_list);
		}

	}

	void Renderer::PassShadowMapDirectional(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == ELightType::Directional);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Shadow Map Pass - Directional Light");

		auto upload_buffer = gfx->GetDynamicAllocator();

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
		shadow_map_barrier.AddTransition(shadow_depth_map->GetNative(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
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

		auto upload_buffer = gfx->GetDynamicAllocator();

		auto const& [V, P] = LightViewProjection_Spot(light, light_bounding_frustum);
		shadow_cbuf_data.lightview = V;
		shadow_cbuf_data.lightviewprojection = V * P;
		shadow_cbuf_data.shadow_map_size = SHADOW_MAP_SIZE;
		shadow_cbuf_data.softness = settings.shadow_softness;
		shadow_cbuf_data.shadow_matrix1 = XMMatrixInverse(nullptr, camera->View()) * shadow_cbuf_data.lightviewprojection;

		shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		shadow_allocation.Update(shadow_cbuf_data);

		ResourceBarrierBatch shadow_map_barrier{};
		shadow_map_barrier.AddTransition(shadow_depth_map->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
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

		auto upload_buffer = gfx->GetDynamicAllocator();

		ResourceBarrierBatch shadow_cubemap_barrier{};
		shadow_cubemap_barrier.AddTransition(shadow_depth_cubemap->GetNative(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
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

		auto upload_buffer = gfx->GetDynamicAllocator();

		ResourceBarrierBatch shadow_cascades_barrier{};
		shadow_cascades_barrier.AddTransition(shadow_depth_cascades->GetNative(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
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
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

		auto shadow_view = reg.view<Mesh, Transform, Visibility>();
		if (!settings.shadow_transparent)
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::DepthMap));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::DepthMap));
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

			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::DepthMap));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::DepthMap));
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

			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::DepthMap_Transparent));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::DepthMap_Transparent));
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

				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i),
					albedo_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(i));

				mesh.Draw(cmd_list);
			}
		}
	}
	
	void Renderer::PassVolumetric(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.volumetric);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Volumetric Lighting Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		if (light.type == ELightType::Directional && !light.casts_shadows)
		{
			ADRIA_LOG(WARNING, "Calling PassVolumetric on a Directional Light that does not cast shadows does not make sense!");
			return;
		}

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Volumetric));
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);
		cmd_list->SetGraphicsRootConstantBufferView(2, shadow_allocation.gpu_address);
		cmd_list->SetGraphicsRootConstantBufferView(3, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { depth_target->SRV(), {} };
		uint32 src_range_sizes[] = { 1,1 };

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(cpu_handles));
		auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
		uint32 dst_range_sizes[] = { (uint32)ARRAYSIZE(cpu_handles) };

		switch (light.type)
		{
		case ELightType::Directional:
			if (light.use_cascades)
			{
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Volumetric_DirectionalCascades));
				cpu_handles[1] = shadow_depth_cascades->SRV();
			}
			else
			{
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Volumetric_Directional));
				cpu_handles[1] = shadow_depth_map->SRV();
			}
			break;
		case ELightType::Spot:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Volumetric_Spot));
			cpu_handles[1] = shadow_depth_map->SRV();
			break;
		case ELightType::Point:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Volumetric_Point));
			cpu_handles[1] = shadow_depth_cubemap->SRV();
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Light Type!");
		}

		device->CopyDescriptors(1, dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(cpu_handles), cpu_handles, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(4, dst_descriptor);

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
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

		if (entities_group.empty()) return;

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Forward));
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		
		for (auto const& [pso, entities] : entities_group)
		{
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(pso));
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
				D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);

				device->CopyDescriptorsSimple(1, dst_descriptor, diffuse_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));
				mesh.Draw(cmd_list);
			}

		}

	}
	void Renderer::PassSky(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Sky Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

		ObjectCBuffer object_cbuf_data{};
		object_cbuf_data.model = DirectX::XMMatrixTranslationFromVector(camera->Position());
		object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		object_allocation.Update(object_cbuf_data);
		
		switch (settings.sky_type)
		{
		case ESkyType::Skybox:
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Skybox));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Skybox));

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

				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, texture_handle,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(2, dst_descriptor);
			}
			break;
		}
		case ESkyType::UniformColor:
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Sky));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::UniformColorSky));
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
			cmd_list->SetGraphicsRootConstantBufferView(2, weather_cbuffer.View(backbuffer_index).BufferLocation);
			break;
		}
		case ESkyType::HosekWilkie:
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Sky));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::HosekWilkieSky));
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);
			cmd_list->SetGraphicsRootConstantBufferView(2, weather_cbuffer.View(backbuffer_index).BufferLocation);
			break;
		}
		default:
			ADRIA_ASSERT(false);
		}

		cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		BindVertexBuffer(cmd_list, cube_vb.get());
		BindIndexBuffer(cmd_list, cube_ib.get());
		cmd_list->DrawIndexedInstanced(cube_ib->GetCount(), 1, 0, 0, 0);
	}
	void Renderer::UpdateOcean(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Ocean Update Pass");
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

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
			initial_spectrum_barrier.AddTransition(ocean_initial_spectrum->GetNative(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			initial_spectrum_barrier.Submit(cmd_list);

			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::InitialSpectrum));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::InitialSpectrum));
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			device->CopyDescriptorsSimple(1, dst_descriptor, ocean_initial_spectrum->UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
			cmd_list->Dispatch(RESOLUTION / 32, RESOLUTION / 32, 1);

			initial_spectrum_barrier.ReverseTransitions();
			initial_spectrum_barrier.Submit(cmd_list);

			settings.recreate_initial_spectrum = false;
		}

		//phase
		{
			ResourceBarrierBatch phase_barriers{};
			phase_barriers.AddTransition(ping_pong_phase_textures[pong_phase]->GetNative(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			phase_barriers.AddTransition(ping_pong_phase_textures[!pong_phase]->GetNative(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			phase_barriers.Submit(cmd_list);

			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Phase));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Phase));
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
				ping_pong_phase_textures[pong_phase]->SRV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

			descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
				ping_pong_phase_textures[!pong_phase]->UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
			cmd_list->Dispatch(RESOLUTION / 32, RESOLUTION / 32, 1);

			pong_phase = !pong_phase;
		}

		//spectrum
		{
			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Spectrum));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Spectrum));
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			D3D12_CPU_DESCRIPTOR_HANDLE srvs[] = { ping_pong_phase_textures[pong_phase]->SRV(), ocean_initial_spectrum->SRV() };
			OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(srvs));
			auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
			uint32 dst_range_sizes[] = { ARRAYSIZE(srvs) };
			uint32 src_range_sizes[] = { 1, 1 };
			device->CopyDescriptors(ARRAYSIZE(dst_range_sizes), dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(src_range_sizes), srvs, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

			descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
				ping_pong_spectrum_textures[pong_spectrum]->UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

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

			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::FFT));
			{
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::FFT_Horizontal));
				for (uint32 p = 1; p < RESOLUTION; p <<= 1)
				{
					ResourceBarrierBatch fft_barriers{};
					fft_barriers.AddTransition(ping_pong_spectrum_textures[!pong_spectrum]->GetNative(),
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					fft_barriers.AddTransition(ping_pong_spectrum_textures[pong_spectrum]->GetNative(),
						D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					fft_barriers.Submit(cmd_list);
					
					fft_cbuf_data.subseq_count = p;
					fft_cbuffer_allocation = upload_buffer->Allocate(GetCBufferSize<FFTCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					fft_cbuffer_allocation.Update(fft_cbuf_data);
					cmd_list->SetComputeRootConstantBufferView(0, fft_cbuffer_allocation.gpu_address);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
						ping_pong_spectrum_textures[pong_spectrum]->SRV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1),
						ping_pong_spectrum_textures[!pong_spectrum]->UAV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index + 1));
					cmd_list->Dispatch(RESOLUTION, 1, 1);

					pong_spectrum = !pong_spectrum;
				}
			}

			{
				cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::FFT_Vertical));
				for (uint32 p = 1; p < RESOLUTION; p <<= 1)
				{
					ResourceBarrierBatch fft_barriers{};
					fft_barriers.AddTransition(ping_pong_spectrum_textures[!pong_spectrum]->GetNative(),
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					fft_barriers.AddTransition(ping_pong_spectrum_textures[pong_spectrum]->GetNative(),
						D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					fft_barriers.Submit(cmd_list);

					fft_cbuf_data.subseq_count = p;
					fft_cbuffer_allocation = upload_buffer->Allocate(GetCBufferSize<FFTCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					fft_cbuffer_allocation.Update(fft_cbuf_data);
					cmd_list->SetComputeRootConstantBufferView(0, fft_cbuffer_allocation.gpu_address);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
						ping_pong_spectrum_textures[pong_spectrum]->SRV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1),
						ping_pong_spectrum_textures[!pong_spectrum]->UAV(),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index + 1));
					cmd_list->Dispatch(RESOLUTION, 1, 1);

					pong_spectrum = !pong_spectrum;

				}
			}
		}

		//normals
		{
			ResourceBarrierBatch normal_barrier{};
			normal_barrier.AddTransition(ocean_normal_map->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			normal_barrier.Submit(cmd_list);

			cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::OceanNormalMap));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::OceanNormalMap));
			cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
				ping_pong_spectrum_textures[pong_spectrum]->SRV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

			descriptor_index = descriptor_allocator->Allocate();
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
				ocean_normal_map->UAV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
			cmd_list->Dispatch(RESOLUTION / 32, RESOLUTION / 32, 1);

			normal_barrier.ReverseTransitions();
			normal_barrier.Submit(cmd_list);
		}
	}
	void Renderer::PassOcean(ID3D12GraphicsCommandList4* cmd_list)
	{
		if (reg.size<Ocean>() == 0) return;

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();
		
		auto skyboxes = reg.view<Skybox>();
		D3D12_CPU_DESCRIPTOR_HANDLE skybox_handle = null_srv_heap->GetHandle(TEXTURECUBE_SLOT);
		for (auto skybox : skyboxes)
		{
			auto const& _skybox = skyboxes.get(skybox);
			if (_skybox.active)
			{
				skybox_handle = texture_manager.CpuDescriptorHandle(_skybox.cubemap_texture);
				break;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE displacement_handle = ping_pong_spectrum_textures[!pong_spectrum]->SRV();

		if (settings.ocean_tesselation)
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::OceanLOD));
			cmd_list->SetPipelineState(
				settings.ocean_wireframe ? RootSigPSOManager::GetPipelineState(EPipelineStateObject::OceanLOD_Wireframe) : 
										   RootSigPSOManager::GetPipelineState(EPipelineStateObject::OceanLOD));
		}
		else
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Ocean));
			cmd_list->SetPipelineState(
				settings.ocean_wireframe ? RootSigPSOManager::GetPipelineState(EPipelineStateObject::Ocean_Wireframe) :
										   RootSigPSOManager::GetPipelineState(EPipelineStateObject::Ocean));
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
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				device->CopyDescriptorsSimple(1, dst_descriptor, displacement_handle,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(4, dst_descriptor);

				descriptor_index = descriptor_allocator->AllocateRange(3);
				dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { ocean_normal_map->SRV(), skybox_handle, texture_manager.CpuDescriptorHandle(foam_handle) };
				D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { dst_descriptor };
				UINT src_range_sizes[] = { 1, 1, 1 };
				UINT dst_range_sizes[] = { 3 };
				device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetGraphicsRootDescriptorTable(5, dst_descriptor);

				settings.ocean_tesselation ? mesh.Draw(cmd_list, D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST) : mesh.Draw(cmd_list);
			}
		}
	}
	void Renderer::PassParticles(ID3D12GraphicsCommandList4* cmd_list)
	{
		if (reg.size<Emitter>() == 0) return;
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Particles Pass");
		SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(gpu_profiler, cmd_list, EProfilerBlock::ParticlesPass, profiler_settings.profile_particles_pass);

		particle_pass.Begin(cmd_list, true);
		auto emitters = reg.view<Emitter>();
		for (auto emitter : emitters)
		{
			Emitter const& emitter_params = emitters.get(emitter);
			particle_renderer.Render(cmd_list, emitter_params, depth_target->SRV(), texture_manager.CpuDescriptorHandle(emitter_params.particle_texture));
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
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

		lens_flare_textures.push_back(depth_target->SRV());

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::LensFlare));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::LensFlare));
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
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1, 1, 1, 1, 1, 1 };
		uint32 dst_range_sizes[] = { 8 };
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_range_sizes), lens_flare_textures.data(), src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		cmd_list->DrawInstanced(7, 1, 0, 0);

		lens_flare_textures.pop_back();
	}
	void Renderer::PassVolumetricClouds(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.clouds);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Volumetric Clouds Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		
		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Clouds));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Clouds));
		
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, weather_cbuffer.View(backbuffer_index).BufferLocation);
		
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(4);
		
		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { clouds_textures[0], clouds_textures[1], clouds_textures[2], depth_target->SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1, 1 };
		uint32 dst_range_sizes[] = { 4 };
		
		
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
		
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		
		cmd_list->DrawInstanced(4, 1, 0, 0);


	}
	void Renderer::PassSSR(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.reflections == EReflections::SSR);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "SSR Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::SSR));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::SSR));

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0]->SRV(), postprocess_textures[!postprocess_index]->SRV(), depth_target->SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1 };
		uint32 dst_range_sizes[] = { 3 };

		
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassDepthOfField(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Depth of Field Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::DOF));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::DOF));

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index]->SRV(), blur_final_texture->SRV(), depth_target->SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1, 1 };
		uint32 dst_range_sizes[] = { 3 };

		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

		if (settings.bokeh) PassDrawBokeh(cmd_list);
	}
	void Renderer::PassGenerateBokeh(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof && settings.bokeh);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Bokeh Generation Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		D3D12_RESOURCE_BARRIER prereset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,  D3D12_RESOURCE_STATE_COPY_DEST);
		cmd_list->ResourceBarrier(1, &prereset_barrier);
		cmd_list->CopyBufferRegion(bokeh_counter.GetNative(), 0, counter_reset_buffer->GetNative(), 0, sizeof(uint32));
		D3D12_RESOURCE_BARRIER postreset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(),D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmd_list->ResourceBarrier(1, &postreset_barrier);

 		cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::BokehGenerate));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::BokehGenerate));
		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(2, compute_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

		D3D12_RESOURCE_BARRIER dispatch_barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(bokeh->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(dispatch_barriers), dispatch_barriers);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index]->SRV(), depth_target->SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1 };
		uint32 dst_range_sizes[] = { 2 };
		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));

		D3D12_CPU_DESCRIPTOR_HANDLE bokeh_uav = bokeh->UAV();
		descriptor_index = descriptor_allocator->Allocate();

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), bokeh_uav,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetHandle(descriptor_index));
		cmd_list->Dispatch((uint32)std::ceil(width / 32.0f), (uint32)std::ceil(height / 32.0f), 1);

		CD3DX12_RESOURCE_BARRIER precopy_barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_target->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer->GetNative(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(precopy_barriers), precopy_barriers);

		cmd_list->CopyBufferRegion(bokeh_indirect_draw_buffer->GetNative(), 0, bokeh_counter.GetNative(), 0, bokeh_counter.GetDesc().size);

		CD3DX12_RESOURCE_BARRIER postcopy_barriers[] = 
		{
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer->GetNative(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_counter.GetNative(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(postcopy_barriers), postcopy_barriers);
	}
	void Renderer::PassDrawBokeh(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof && settings.bokeh);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Bokeh Draw Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

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

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Bokeh));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Bokeh));

		OffsetType i = descriptor_allocator->AllocateRange(2);

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i),
			bokeh->SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1),
			bokeh_descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(i));
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(i + 1));

		cmd_list->IASetVertexBuffers(0, 0, nullptr);
		cmd_list->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		cmd_list->ExecuteIndirect(bokeh_command_signature.Get(), 1, bokeh_indirect_draw_buffer->GetNative(), 0,
			nullptr, 0);

	}
	void Renderer::PassBloom(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.bloom);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Bloom Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		D3D12_RESOURCE_BARRIER extract_barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(bloom_extract_texture->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[!postprocess_index]->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
		};
		cmd_list->ResourceBarrier(ARRAYSIZE(extract_barriers), extract_barriers);

		cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::BloomExtract));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::BloomExtract));
		cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);
		
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = postprocess_textures[!postprocess_index]->SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
		
		++descriptor_index;
		cpu_descriptor = bloom_extract_texture->UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
		
		cmd_list->Dispatch((uint32)std::ceil(postprocess_textures[!postprocess_index]->GetDesc().width / 32),
			(uint32)std::ceil(postprocess_textures[!postprocess_index]->GetDesc().height / 32), 1);

		D3D12_RESOURCE_BARRIER combine_barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(bloom_extract_texture->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[postprocess_index]->GetNative(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		};

		cmd_list->ResourceBarrier(ARRAYSIZE(combine_barriers), combine_barriers);

		GenerateMips(cmd_list, *bloom_extract_texture);

		cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::BloomCombine));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::BloomCombine));

		descriptor_index = descriptor_allocator->AllocateRange(3);
		cpu_descriptor = postprocess_textures[!postprocess_index]->SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//++descriptor_index;
		cpu_descriptor = bloom_extract_texture->SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

		descriptor_index += 2;
		cpu_descriptor = postprocess_textures[postprocess_index]->UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

		cmd_list->Dispatch((uint32)std::ceil(postprocess_textures[!postprocess_index]->GetDesc().width / 32),
			(uint32)std::ceil(postprocess_textures[!postprocess_index]->GetDesc().height / 32), 1);


		D3D12_RESOURCE_BARRIER final_barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[!postprocess_index]->GetNative(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(postprocess_textures[postprocess_index]->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET),
		};

		cmd_list->ResourceBarrier(ARRAYSIZE(final_barriers), final_barriers);

	}
	void Renderer::PassMotionBlur(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.motion_blur);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Motion Blur Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::MotionBlur));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::MotionBlur));

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index]->SRV(), velocity_buffer->SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1 };
		uint32 dst_range_sizes[] = { 2 };

		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassFog(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.fog);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Fog Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Fog));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Fog));

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = {postprocess_textures[!postprocess_index]->SRV(), depth_target->SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetHandle(descriptor_index) };
		uint32 src_range_sizes[] = { 1, 1 };
		uint32 dst_range_sizes[] = { 2 };

		device->CopyDescriptors(ARRAYSIZE(dst_ranges), dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	
	void Renderer::PassGodRays(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.god_rays);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "God Rays Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

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

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::GodRays));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::GodRays));

		cmd_list->SetGraphicsRootConstantBufferView(0, light_allocation.gpu_address);

		OffsetType descriptor_index = descriptor_allocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = sun_target->SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassVelocityBuffer(ID3D12GraphicsCommandList4* cmd_list)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Velocity Buffer Pass");

		if (!settings.motion_blur && !(settings.anti_aliasing & AntiAliasing_TAA)) return;

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		ResourceBarrierBatch velocity_barrier{};
		velocity_barrier.AddTransition(velocity_buffer->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		velocity_barrier.Submit(cmd_list);

		velocity_buffer_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::VelocityBuffer));
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::VelocityBuffer));

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);
			
			OffsetType descriptor_index = descriptor_allocator->AllocateRange(1);
			device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), 
				depth_target->SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
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
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::ToneMap));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::ToneMap));

		cmd_list->SetGraphicsRootConstantBufferView(0, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = postprocess_textures[!postprocess_index]->SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

	}
	void Renderer::PassFXAA(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.anti_aliasing & AntiAliasing_FXAA);
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "FXAA Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		ResourceBarrierBatch fxaa_barrier{};
		fxaa_barrier.AddTransition(ldr_render_target->GetNative(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		fxaa_barrier.Submit(cmd_list);

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::FXAA));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::FXAA));

		OffsetType descriptor_index = descriptor_allocator->Allocate();

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), ldr_render_target->SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

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
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::TAA));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::TAA));

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), postprocess_textures[!postprocess_index]->SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), prev_hdr_render_target->SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 2), velocity_buffer->SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);
	}

	void Renderer::DrawSun(ID3D12GraphicsCommandList4* cmd_list, tecs::entity sun)
	{
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Sun Pass");

		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

		ResourceBarrierBatch sun_barrier{};
		sun_barrier.AddTransition(sun_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		sun_barrier.AddTransition(depth_target->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		sun_barrier.Submit(cmd_list);

		sun_target->GetNative()->SetName(L"Sun target");

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = sun_target->RTV();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = depth_target->DSV();
		float32 black[4] = { 0.0f };
		cmd_list->ClearRenderTargetView(rtv, black, 0, nullptr);
		cmd_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Forward));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Sun));
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
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);

			device->CopyDescriptorsSimple(1, dst_descriptor, diffuse_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetHandle(descriptor_index));

			mesh.Draw(cmd_list);
		}
		cmd_list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

		sun_barrier.ReverseTransitions();
		sun_barrier.Submit(cmd_list);
	}
	void Renderer::BlurTexture(ID3D12GraphicsCommandList4* cmd_list, Texture const& texture)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		ResourceBarrierBatch barrier{};
		barrier.AddTransition(blur_intermediate_texture->GetNative(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		barrier.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Blur));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Blur_Horizontal));

		cmd_list->SetComputeRootConstantBufferView(0, compute_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = texture.SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

		++descriptor_index;
		cpu_descriptor = blur_intermediate_texture->UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

		cmd_list->Dispatch((uint32)std::ceil(texture.GetDesc().width / 1024.0f), texture.GetDesc().height, 1);

		barrier.ReverseTransitions();
		barrier.AddTransition(blur_final_texture->GetNative(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		barrier.Submit(cmd_list);

		//vertical pass
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Blur_Vertical));
		descriptor_index = descriptor_allocator->AllocateRange(2);

		cpu_descriptor = blur_intermediate_texture->SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

		++descriptor_index;
		cpu_descriptor = blur_final_texture->UAV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

		cmd_list->Dispatch(texture.GetDesc().width, (UINT)std::ceil(texture.GetDesc().height / 1024.0f), 1);

		barrier.Clear();
		barrier.AddTransition(blur_final_texture->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		barrier.Submit(cmd_list);
	}
	void Renderer::CopyTexture(ID3D12GraphicsCommandList4* cmd_list, Texture const& texture, EBlendMode mode)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Copy));
		
		switch (mode)
		{
		case EBlendMode::None:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Copy));
			break;
		case EBlendMode::AlphaBlend:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Copy_AlphaBlend));
			break;
		case EBlendMode::AdditiveBlend:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Copy_AdditiveBlend));
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
		}

		OffsetType descriptor_index = descriptor_allocator->Allocate();

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), texture.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::GenerateMips(ID3D12GraphicsCommandList4* cmd_list, Texture const& _texture,
		D3D12_RESOURCE_STATES start_state, D3D12_RESOURCE_STATES end_state)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		ID3D12Resource* texture = _texture.GetNative();

		//Set root signature, pso and descriptor heap
		cmd_list->SetComputeRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::GenerateMips));
		cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::GenerateMips));

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
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle1 = descriptor_allocator->GetHandle(i);
			D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle1 = descriptor_allocator->GetHandle(i);

			src_srv_desc.Format = tex_desc.Format;
			src_srv_desc.Texture2D.MipLevels = 1;
			src_srv_desc.Texture2D.MostDetailedMip = top_mip;
			device->CreateShaderResourceView(texture, &src_srv_desc, cpu_handle1);


			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle2 = descriptor_allocator->GetHandle(i + 1);
			D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle2 = descriptor_allocator->GetHandle(i + 1);
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
	void Renderer::AddTextures(ID3D12GraphicsCommandList4* cmd_list, Texture const& texture1, Texture const& texture2, EBlendMode mode)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(RootSigPSOManager::GetRootSignature(ERootSignature::Add));

		switch (mode)
		{
		case EBlendMode::None:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Add));
			break;
		case EBlendMode::AlphaBlend:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Add_AlphaBlend));
			break;
		case EBlendMode::AdditiveBlend:
			cmd_list->SetPipelineState(RootSigPSOManager::GetPipelineState(EPipelineStateObject::Add_AdditiveBlend));
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
		}

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index), texture1.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1), texture2.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
}
