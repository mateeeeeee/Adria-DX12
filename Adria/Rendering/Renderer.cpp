#include "../Threading/TaskSystem.h"
#include "../Utilities/Random.h"
#include "Renderer.h"
#include <algorithm>
#include <iterator>
#include "Renderer.h"
#include "Camera.h"
#include "Components.h"
#include "../Core/Window.h"
#include "../Utilities/Timer.h"
#include "../Math/Constants.h"
#include "../Logging/Logger.h"
#include "../Editor/GUI.h"


using namespace DirectX;

using namespace Microsoft::WRL;

namespace adria
{
	//helpers
	namespace
	{
		constexpr u32 SHADOW_MAP_SIZE = 2048;
		constexpr u32 SHADOW_CUBE_SIZE = 512;
		constexpr u32 SHADOW_CASCADE_MAP_SIZE = 1024;
		constexpr u32 CASCADE_COUNT = 3;

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
			f32 l = sphere_centerLS.x - scene_bounding_sphere.Radius;
			f32 b = sphere_centerLS.y - scene_bounding_sphere.Radius;
			f32 n = sphere_centerLS.z - scene_bounding_sphere.Radius;
			f32 r = sphere_centerLS.x + scene_bounding_sphere.Radius;
			f32 t = sphere_centerLS.y + scene_bounding_sphere.Radius;
			f32 f = sphere_centerLS.z + scene_bounding_sphere.Radius;

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
			for (u32 i = 0; i < corners.size(); ++i)
			{
				frustumCenter = frustumCenter + XMLoadFloat3(&corners[i]);
			}
			frustumCenter /= static_cast<f32>(corners.size());

			f32 radius = 0.0f;

			for (u32 i = 0; i < corners.size(); ++i)
			{
				f32 dist = DirectX::XMVectorGetX(XMVector3Length(DirectX::XMLoadFloat3(&corners[i]) - frustumCenter));
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

			f32 l = minE.x;
			f32 b = minE.y;
			f32 n = minE.z; // -farFactor * radius;
			f32 r = maxE.x;
			f32 t = maxE.y;
			f32 f = maxE.z * 1.5f;

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
			ADRIA_ASSERT(light.type == LightType::eSpot);

			XMVECTOR light_dir = XMVector3Normalize(light.direction);

			XMVECTOR light_pos = light.position;

			XMVECTOR target_pos = light_pos + light_dir * light.range;

			static const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			XMMATRIX V = XMMatrixLookAtLH(light_pos, target_pos, up);

			static const f32 shadow_near = 0.5f;

			f32 fov_angle = 2.0f * acos(light.outer_cosine);

			XMMATRIX P = XMMatrixPerspectiveFovLH(fov_angle, 1.0f, shadow_near, light.range);

			cull_frustum = BoundingFrustum(P);
			cull_frustum.Transform(cull_frustum, XMMatrixInverse(nullptr, V));

			return { V,P };
		}

		std::pair<DirectX::XMMATRIX, DirectX::XMMATRIX> LightViewProjection_Point(Light const& light, u32 face_index,
			BoundingFrustum& cull_frustum)
		{
			static f32 const shadow_near = 0.5f;
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

		std::array<XMMATRIX, CASCADE_COUNT> RecalculateProjectionMatrices(Camera const& camera, f32 split_lambda, std::array<f32, CASCADE_COUNT>& split_distances)
		{

			f32 camera_near = camera.Near();
			f32 camera_far = camera.Far();
			f32 fov = camera.Fov();
			f32 ar = camera.AspectRatio();

			f32 f = 1.0f / CASCADE_COUNT;

			for (u32 i = 0; i < split_distances.size(); i++)
			{
				f32 fi = (i + 1) * f;
				f32 l = camera_near * pow(camera_far / camera_near, fi);
				f32 u = camera_near + (camera_far - camera_near) * fi;
				split_distances[i] = l * split_lambda + u * (1.0f - split_lambda);
			}

			std::array<XMMATRIX, CASCADE_COUNT> projectionMatrices{};
			projectionMatrices[0] = DirectX::XMMatrixPerspectiveFovLH(fov, ar, camera_near, split_distances[0]);
			for (u32 i = 1; i < projectionMatrices.size(); ++i)
				projectionMatrices[i] = DirectX::XMMatrixPerspectiveFovLH(fov, ar, split_distances[i - 1], split_distances[i]);

			return projectionMatrices;
		}

		std::pair<DirectX::XMMATRIX, DirectX::XMMATRIX> LightViewProjection_Cascades(Light const& light, Camera const& camera,
			XMMATRIX projection_matrix,
			BoundingBox& cull_box)
		{
			static f32 const farFactor = 1.5f;
			static f32 const lightDistanceFactor = 4.0f;

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
			for (u32 i = 0; i < corners.size(); ++i)
			{
				frustumCenter = frustumCenter + XMLoadFloat3(&corners[i]);
			}
			frustumCenter /= static_cast<f32>(corners.size());

			f32 radius = 0.0f;

			for (u32 i = 0; i < corners.size(); ++i)
			{
				f32 dist = XMVectorGetX(XMVector3Length(XMLoadFloat3(&corners[i]) - frustumCenter));
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

			f32 l = minE.x;
			f32 b = minE.y;
			f32 n = minE.z - farFactor * radius;
			f32 r = maxE.x;
			f32 t = maxE.y;
			f32 f = maxE.z * farFactor;

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
		
		f32 GaussianDistribution(f32 x, f32 sigma)
		{
			static const f32 square_root_of_two_pi = sqrt(pi_times_2<f32>);
			return expf((-x * x) / (2 * sigma * sigma)) / (sigma * square_root_of_two_pi);
		}
		template<i16 N>
		std::array<f32, 2 * N + 1> GaussKernel(f32 sigma)
		{
			std::array<f32, 2 * N + 1> gauss{};
			f32 sum = 0.0f;
			for (i16 i = -N; i <= N; ++i)
			{
				gauss[i + N] = GaussianDistribution(i * 1.0f, sigma);
				sum += gauss[i + N];
			}
			for (i16 i = -N; i <= N; ++i)
			{
				gauss[i + N] /= sum;
			}

			return gauss;
		}
		
	}

	using namespace tecs;


	Renderer::Renderer(tecs::registry& reg, GraphicsCoreDX12* gfx, u32 width, u32 height)
		: reg(reg), gfx(gfx), width(width), height(height), texture_manager(gfx, 1000), backbuffer_count(gfx->BackbufferCount()),
		frame_cbuffer(gfx->Device(), backbuffer_count), postprocess_cbuffer(gfx->Device(), backbuffer_count),
		compute_cbuffer(gfx->Device(), backbuffer_count), weather_cbuffer(gfx->Device(), backbuffer_count),
		clusters(gfx->Device(), CLUSTER_COUNT, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		light_counter(gfx->Device(), 1, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		light_list(gfx->Device(), CLUSTER_COUNT * CLUSTER_MAX_LIGHTS, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		light_grid(gfx->Device(), CLUSTER_COUNT, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{

		LoadShaders();
		CreatePipelineStateObjects();
		CreateDescriptorHeaps();
		CreateViews(width, height);
		CreateRenderPasses(width, height);

	}
	Renderer::~Renderer()
	{
		gfx->WaitForGPU();
		reg.clear();
	}

	void Renderer::NewFrame(Camera const* camera)
	{
		this->camera = camera;
		backbuffer_index = gfx->BackbufferIndex();

		static f32 _near = 0.0f, _far = 0.0f, _fov = 0.0f, _ar = 0.0f;
		if (fabs(_near - camera->Near()) > 1e-4 || fabs(_far - camera->Far()) > 1e-4 || fabs(_fov - camera->Fov()) > 1e-4
			|| fabs(_ar - camera->AspectRatio()) > 1e-4)
		{
			_near = camera->Near();
			_far = camera->Far();
			_fov = camera->Fov();
			_ar = camera->AspectRatio();
			recreate_clusters = true;
		}
	}
	void Renderer::Update(f32 dt)
	{
		UpdateConstantBuffers(dt);
		CameraFrustumCulling();
	}
	void Renderer::Render(RendererSettings const& _settings)
	{
		settings = _settings;
		if (settings.ibl && !ibl_textures_generated) CreateIBLTextures();

		auto cmd_list = gfx->DefaultCommandList();

		ResourceBarriers main_barrier{};
		hdr_render_target.Transition(main_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		main_barrier.Submit(cmd_list);

		ResourceBarriers depth_barrier{};
		depth_stencil_target.Transition(depth_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		depth_barrier.Submit(cmd_list);

		PassGBuffer(cmd_list);

		if (settings.ssao)
		{
			depth_barrier.ReverseTransitions();
			depth_barrier.Submit(cmd_list);

			PassSSAO(cmd_list);

			depth_barrier.ReverseTransitions();
			depth_barrier.Submit(cmd_list);
		}

		PassAmbient(cmd_list);

		PassDeferredLighting(cmd_list);

		if (settings.use_tiled_deferred) PassDeferredTiledLighting(cmd_list);
		else if (settings.use_clustered_deferred) PassDeferredClusteredLighting(cmd_list);

		PassForward(cmd_list);

		main_barrier.Merge(std::move(depth_barrier));
		main_barrier.ReverseTransitions();
		main_barrier.Submit(cmd_list);

		PassPostprocess(cmd_list);
	}

	void Renderer::Render_Multithreaded(RendererSettings const& _settings)
	{
		settings = _settings;
		if (settings.ibl && !ibl_textures_generated) CreateIBLTextures();

		auto cmd_list = gfx->DefaultCommandList();
		auto gbuf_cmd_list = gfx->NewCommandList();
		auto deferred_cmd_list = gfx->NewCommandList();
		auto postprocess_cmd_list = gfx->NewCommandList();

		hdr_render_target.Resource()->SetName(L"hdr rt");
		depth_stencil_target.Resource()->SetName(L"depth rt");


		auto gbuf_ambient_fut = TaskSystem::Submit([this, gbuf_cmd_list]()
		{
			gbuf_cmd_list->SetName(L"gbuf_ambient");

			D3D12_RESOURCE_BARRIER barriers[] = 
			{
				CD3DX12_RESOURCE_BARRIER::Transition(hdr_render_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
			};

			gbuf_cmd_list->ResourceBarrier(_countof(barriers), barriers);

			PassGBuffer(gbuf_cmd_list);

			if (settings.ssao)
			{
				D3D12_RESOURCE_BARRIER pre_ssao_barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_target.Resource(),D3D12_RESOURCE_STATE_DEPTH_WRITE,  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				};
				gbuf_cmd_list->ResourceBarrier(_countof(pre_ssao_barriers), pre_ssao_barriers);


				PassSSAO(gbuf_cmd_list);

				D3D12_RESOURCE_BARRIER  post_ssao_barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
				};
				gbuf_cmd_list->ResourceBarrier(_countof(post_ssao_barriers), post_ssao_barriers);

			}
			PassAmbient(gbuf_cmd_list);
		}
		);

		auto deferred_fut = TaskSystem::Submit([this, deferred_cmd_list]()
		{
			
			deferred_cmd_list->SetName(L"deferred");
			PassDeferredLighting(deferred_cmd_list);

			if (settings.use_tiled_deferred) PassDeferredTiledLighting(deferred_cmd_list);
			else if (settings.use_clustered_deferred) PassDeferredClusteredLighting(deferred_cmd_list);

			PassForward(deferred_cmd_list);
		}
		);

		auto postprocess_fut = TaskSystem::Submit([this, postprocess_cmd_list]()
		{
			
			postprocess_cmd_list->SetName(L"postprocess");
			
			D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(hdr_render_target.Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_target.Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			};

			postprocess_cmd_list->ResourceBarrier(_countof(barriers), barriers);

			PassPostprocess(postprocess_cmd_list);

		});

		gbuf_ambient_fut.wait();
		deferred_fut.wait();
		postprocess_fut.wait();
	}
	
	void Renderer::ResolveToBackbuffer()
	{
		auto cmd_list = gfx->NewCommandList(); 
		D3D12_VIEWPORT vp{};
		vp.Width = (f32)width;
		vp.Height = (f32)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		cmd_list->RSSetViewports(1, &vp);
		D3D12_RECT rect{};
		rect.bottom = (i64)height;
		rect.left = 0;
		rect.right = (i64)width;
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
		auto cmd_list = gfx->NewCommandList(); 
		if (settings.anti_aliasing & AntiAliasing_FXAA)
		{
			fxaa_render_pass.Begin(cmd_list);
			PassToneMap(cmd_list);
			fxaa_render_pass.End(cmd_list);

			ResourceBarriers offscreen_barrier{};
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
			ResourceBarriers offscreen_barrier{};
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
	void Renderer::OnResize(u32 _width, u32 _height)
	{
		width = _width, height = _height;
		if (width != 0 || height != 0)
		{
			CreateViews(width, height);
			CreateRenderPasses(width, height);
		}
	}
	void Renderer::LoadTextures()
	{
		ID3D12Resource* noise_upload_texture = nullptr; //keep it alive until call to execute
		//upload data to noise texture
		{
			const u64 upload_buffer_size = GetRequiredIntermediateSize(noise_texture.Resource(), 0, 1);

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
			BREAK_IF_FAILED(gfx->Device()->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&noise_upload_texture)));

			RealRandomGenerator rand_float{ 0.0f, 1.0f };
			std::vector<f32> random_texture_data;
			for (i32 i = 0; i < 8 * 8; i++)
			{

				random_texture_data.push_back(rand_float()); //2 * rand_float() - 1
				random_texture_data.push_back(rand_float());
				random_texture_data.push_back(0.0f);
				random_texture_data.push_back(1.0f);
			}

			D3D12_SUBRESOURCE_DATA data{};
			data.pData = random_texture_data.data();
			data.RowPitch = 8 * 4 * sizeof(f32);
			data.SlicePitch = 0;

			UpdateSubresources(gfx->DefaultCommandList(), noise_texture.Resource(), noise_upload_texture, 0, 0, 1, &data);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(noise_texture.Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			gfx->DefaultCommandList()->ResourceBarrier(1, &barrier);

			noise_texture.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));

		}

		gfx->AddToReleaseQueue(noise_upload_texture);

		//lens flare
		texture_manager.SetMipMaps(false);
		{
			TEXTURE_HANDLE tex_handle{};

			ResourceBarriers barrier{};

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

			barrier.Submit(gfx->DefaultCommandList());
		}

		//clouds
		{
			clouds_textures.push_back(texture_manager.CpuDescriptorHandle(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\weather.dds")));
			clouds_textures.push_back(texture_manager.CpuDescriptorHandle(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds")));
			clouds_textures.push_back(texture_manager.CpuDescriptorHandle(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\worley.dds")));
		}

		//this doesnt belong here, move it somewhere else later
		//bokeh indirect draw buffer & zero counter buffer
		{
			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.SampleDesc.Count = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Width = 4 * sizeof(u32);
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			BREAK_IF_FAILED(gfx->Device()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&bokeh_indirect_draw_buffer)));
		}

		//bokeh upload indirect draw buffer
		ID3D12Resource* bokeh_upload_buffer = nullptr;
		{
			hex_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Hex.dds");
			oct_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Oct.dds");
			circle_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Circle.dds");
			cross_bokeh_handle = texture_manager.LoadTexture(L"Resources/Textures/bokeh/Bokeh_Cross.dds");

			const u64 upload_buffer_size = GetRequiredIntermediateSize(bokeh_indirect_draw_buffer.Get(), 0, 1);
			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
			BREAK_IF_FAILED(gfx->Device()->CreateCommittedResource(
				&heap_properties,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&bokeh_upload_buffer)));

			u32 init_data[] = { 0,1,0,0 };

			D3D12_SUBRESOURCE_DATA data{};
			data.pData = init_data;
			data.RowPitch = sizeof(init_data);
			data.SlicePitch = 0;

			UpdateSubresources(gfx->DefaultCommandList(), bokeh_indirect_draw_buffer.Get(), bokeh_upload_buffer, 0, 0, 1, &data);

			D3D12_INDIRECT_ARGUMENT_DESC args[1];
			args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

			// Data structure to match the command signature used for ExecuteIndirect.
			
			D3D12_COMMAND_SIGNATURE_DESC command_signature_desc{};
			command_signature_desc.NumArgumentDescs = 1;
			command_signature_desc.pArgumentDescs = args;
			command_signature_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);

			BREAK_IF_FAILED(gfx->Device()->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&bokeh_command_signature)));


			// Allocate a buffer that can be used to reset the UAV counters and initialize it to 0.
			auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(u32));
			auto upload_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			BREAK_IF_FAILED(gfx->Device()->CreateCommittedResource(
				&upload_heap,
				D3D12_HEAP_FLAG_NONE,
				&buffer_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&counter_reset_buffer)));

			u8* mapped_reset_buffer = nullptr;
			CD3DX12_RANGE read_range(0, 0);        // We do not intend to read from this resource on the CPU.
			BREAK_IF_FAILED(counter_reset_buffer->Map(0, &read_range, reinterpret_cast<void**>(&mapped_reset_buffer)));
			ZeroMemory(mapped_reset_buffer, sizeof(u32));
			counter_reset_buffer->Unmap(0, nullptr);
		}
		gfx->AddToReleaseQueue(bokeh_upload_buffer);

		texture_manager.SetMipMaps(true);
	}
	TextureManager& Renderer::GetTextureManager()
	{
		return texture_manager;
	}
	Texture2D Renderer::GetOffscreenTexture() const
	{
		return offscreen_ldr_target;
	}

	
	////////////////////////////////////////////////////////////////////////////
	////////////////////////////////PRIVATE/////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////

	void Renderer::LoadShaders()
	{
		
		//misc
		{
			ShaderBlob vs_blob, ps_blob;
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SkyboxVS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SkyboxPS.cso", ps_blob);
			shader_map[VS_Skybox] = vs_blob;
			shader_map[PS_Skybox] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TextureVS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TexturePS.cso", ps_blob);
			shader_map[VS_Texture] = vs_blob;
			shader_map[PS_Texture] = ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SunVS.cso", vs_blob);
			shader_map[VS_Sun] = vs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BillboardVS.cso", vs_blob);
			shader_map[VS_Billboard] = vs_blob;
		}

		//postprocess
		{
			ShaderBlob vs_blob, ps_blob, gs_blob;

			
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ScreenQuadVS.cso", vs_blob);
			shader_map[VS_ScreenQuad] = vs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/SSAO_PS.cso", vs_blob);
			shader_map[PS_Ssao] = vs_blob;

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
		}

		//gbuffer 
		{
			ShaderBlob vs_blob, ps_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GeometryPassPBR_VS.cso", vs_blob);
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/GeometryPassPBR_PS.cso", ps_blob);

			shader_map[VS_GBufferPBR] = vs_blob;
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
			shader_map[PS_AmbientPBR_SSAO] = ps_blob;

			shader_info_ps.defines.clear();
			shader_info_ps.defines.emplace_back(L"IBL", L"1");
			ShaderUtility::CompileShader(shader_info_ps, ps_blob);
			shader_map[PS_AmbientPBR_IBL] = ps_blob;

			shader_info_ps.defines.emplace_back(L"SSAO", L"1");
			ShaderUtility::CompileShader(shader_info_ps, ps_blob);
			shader_map[PS_AmbientPBR_SSAO_IBL] = ps_blob;
			
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

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BloomExtractCS.cso", cs_blob);
			shader_map[CS_BloomExtract] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/TiledLightingCS.cso", cs_blob);
			shader_map[CS_TiledLighting] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterBuildingCS.cso", cs_blob);
			shader_map[CS_ClusterBuilding] = cs_blob;

			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/ClusterCullingCS.cso", cs_blob);
			shader_map[CS_ClusterCulling] = cs_blob;


			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/BokehCS.cso", cs_blob);
			shader_map[CS_BokehGenerate] = cs_blob;
		}

		
	}
	void Renderer::CreatePipelineStateObjects()
	{
		
		auto device = gfx->Device();

		
		//root sigs (written in hlsl)
		{
			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Skybox].GetPointer(), shader_map[PS_Skybox].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eSkybox].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_ToneMap].GetPointer(), shader_map[PS_ToneMap].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eToneMap].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Fxaa].GetPointer(), shader_map[PS_Fxaa].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eFXAA].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Taa].GetPointer(), shader_map[PS_Taa].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eTAA].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_GBufferPBR].GetPointer(), shader_map[PS_GBufferPBR].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eGbufferPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_AmbientPBR].GetPointer(), shader_map[PS_AmbientPBR].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eAmbientPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_LightingPBR].GetPointer(), shader_map[PS_LightingPBR].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eLightingPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_ClusteredLightingPBR].GetPointer(), shader_map[PS_ClusteredLightingPBR].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eClusteredLightingPBR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_DepthMap].GetPointer(), shader_map[PS_DepthMap].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eDepthMap].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_DepthMap_Transparent].GetPointer(), shader_map[PS_DepthMap_Transparent].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eDepthMap_Transparent].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Volumetric_Directional].GetPointer(), shader_map[PS_Volumetric_Directional].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eVolumetric].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Texture].GetPointer(), shader_map[PS_Texture].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eForward].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Ssr].GetPointer(), shader_map[PS_Ssr].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eSSR].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_GodRays].GetPointer(), shader_map[PS_GodRays].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eGodRays].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_LensFlare].GetPointer(), shader_map[PS_LensFlare].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eLensFlare].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Dof].GetPointer(), shader_map[PS_Dof].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eDof].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Add].GetPointer(), shader_map[PS_Add].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eAdd].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Fog].GetPointer(), shader_map[PS_Fog].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eFog].GetAddressOf())));

			//ID3D12VersionedRootSignatureDeserializer* drs = nullptr;
			//D3D12CreateVersionedRootSignatureDeserializer(shader_map[PS_Add].GetPointer(), shader_map[PS_Add].GetLength(), IID_PPV_ARGS(&drs));
			//D3D12_VERSIONED_ROOT_SIGNATURE_DESC const* desc = drs->GetUnconvertedRootSignatureDesc();

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_VolumetricClouds].GetPointer(), shader_map[PS_VolumetricClouds].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eClouds].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_MotionBlur].GetPointer(), shader_map[PS_MotionBlur].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eMotionBlur].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[PS_Bokeh].GetPointer(), shader_map[PS_Bokeh].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eBokeh].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_TiledLighting].GetPointer(), shader_map[CS_TiledLighting].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eTiledLighting].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ClusterBuilding].GetPointer(), shader_map[CS_ClusterBuilding].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eClusterBuilding].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_ClusterCulling].GetPointer(), shader_map[CS_ClusterCulling].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eClusterCulling].GetAddressOf())));

			BREAK_IF_FAILED(device->CreateRootSignature(0, shader_map[CS_BokehGenerate].GetPointer(), shader_map[CS_BokehGenerate].GetLength(),
				IID_PPV_ARGS(rs_map[RootSig::eBokehGenerate].GetAddressOf())));


			rs_map[RootSig::eCopy] = rs_map[RootSig::eFXAA];
		}
		//root sigs (written in C++)
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};

			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
				feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

			//ssao
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
				rootSignatureDesc.Init_1_1((u32)root_parameters.size(), root_parameters.data(), (u32)samplers.size(), samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, feature_data.HighestVersion, &signature, &error);
				if (error) OutputDebugStringA((char*)error->GetBufferPointer());

				BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[RootSig::eSSAO])));

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
				rootSignatureDesc.Init_1_1((u32)root_parameters.size(), root_parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, feature_data.HighestVersion, &signature, &error);
				if (error) OutputDebugStringA((char*)error->GetBufferPointer());

				BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rs_map[RootSig::eBlur])));
				
			}

			rs_map[RootSig::eBloomExtract] = rs_map[RootSig::eBlur];
		}
		//pso
		{

			//skybox
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Skybox], input_layout);
				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[RootSig::eSkybox].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eSkybox])));
			}

			//tonemap
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);
				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[RootSig::eToneMap].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eToneMap])));

			}

			//fxaa
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);

				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[RootSig::eFXAA].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eFXAA])));
			}

			//taa
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], input_layout);

				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[RootSig::eTAA].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eTAA])));
			}

			//gbuffer
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_GBufferPBR], input_layout);

				//no emissive
				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[RootSig::eGbufferPBR].Get();
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
				pso_desc.NumRenderTargets = static_cast<u32>(3);
				pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
				pso_desc.SampleDesc.Count = 1;
				pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eGbufferPBR])));
			}

			//ambient 
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = { nullptr, 0u };
				pso_desc.pRootSignature = rs_map[RootSig::eAmbientPBR].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eAmbientPBR])));

				pso_desc.PS = shader_map[PS_AmbientPBR_SSAO];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eAmbientPBR_SSAO])));

				pso_desc.PS = shader_map[PS_AmbientPBR_IBL];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eAmbientPBR_IBL])));

				pso_desc.PS = shader_map[PS_AmbientPBR_SSAO_IBL];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eAmbientPBR_SSAO_IBL])));

			}

			//lighting & clustered lighting
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = { nullptr, 0u };
				pso_desc.pRootSignature = rs_map[RootSig::eLightingPBR].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eLightingPBR])));


				pso_desc.pRootSignature = rs_map[RootSig::eClusteredLightingPBR].Get();
				pso_desc.PS = shader_map[PS_ClusteredLightingPBR];

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eClusteredLightingPBR])));
			}

			//clustered building and culling
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[RootSig::eClusterBuilding].Get();
				pso_desc.CS = shader_map[CS_ClusterBuilding];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eClusterBuilding])));

				pso_desc.pRootSignature = rs_map[RootSig::eClusterCulling].Get();
				pso_desc.CS = shader_map[CS_ClusterCulling];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eClusterCulling])));

			}

			//tiled lighting
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[RootSig::eTiledLighting].Get();
				pso_desc.CS = shader_map[CS_TiledLighting];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eTiledLighting])));
			}

			//depth maps
			{
				InputLayout il;

				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_DepthMap], il);


				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[RootSig::eDepthMap].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eDepthMap])));

				//InputLayout il2;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_DepthMap_Transparent], il);
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[RootSig::eDepthMap_Transparent].Get();
				pso_desc.VS = shader_map[VS_DepthMap_Transparent];
				pso_desc.PS = shader_map[PS_DepthMap_Transparent];

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eDepthMap_Transparent])));

			}

			//volumetric lighting
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.InputLayout = { nullptr, 0u };
				pso_desc.pRootSignature = rs_map[RootSig::eVolumetric].Get();
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
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eVolumetric_Directional])));

				pso_desc.PS = shader_map[PS_Volumetric_DirectionalCascades];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eVolumetric_DirectionalCascades])));

				pso_desc.PS = shader_map[PS_Volumetric_Spot];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eVolumetric_Spot])));

				pso_desc.PS = shader_map[PS_Volumetric_Point];
				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eVolumetric_Point])));

			}
			
			//forward
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				InputLayout input_layout;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_Sun], input_layout);
				pso_desc.InputLayout = input_layout;
				pso_desc.pRootSignature = rs_map[RootSig::eForward].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eSun])));

			}

			//ssao
			{

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eSSAO].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eSSAO])));

			}

			//ssr
			{

				
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eSSR].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eSSR])));
			}

			//god rays
			{
				InputLayout il;
				ShaderUtility::CreateInputLayoutWithReflection(shader_map[VS_ScreenQuad], il);

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = il;
				pso_desc.pRootSignature = rs_map[RootSig::eGodRays].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eGodRays])));
			}

			//lens flare
			{
				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr,0 };
				pso_desc.pRootSignature = rs_map[RootSig::eLensFlare].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eLensFlare])));
			}

			//blur
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[RootSig::eBlur].Get();
				pso_desc.CS = shader_map[CS_Blur_Horizontal];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eBlur_Horizontal])));

				pso_desc.CS = shader_map[CS_Blur_Vertical];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eBlur_Vertical])));
			}

			//bloom extract
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = rs_map[RootSig::eBloomExtract].Get();
				pso_desc.CS = shader_map[CS_BloomExtract];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eBloomExtract])));
			}

			//copy
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eCopy].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eCopy])));

				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eCopy_AlphaBlend])));


				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eCopy_AdditiveBlend])));
			}

			//add
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};

				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eAdd].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eAdd])));

				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eAdd_AlphaBlend])));


				pso_desc.BlendState.RenderTarget[0].BlendEnable = true;
				pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
				pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eAdd_AdditiveBlend])));
			}

			//fog
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};

				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eFog].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eFog])));

			}

			//dof
			{
				
				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eDof].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eDof])));
			}

			//bokeh 
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc = {};
				compute_pso_desc.pRootSignature = rs_map[RootSig::eBokehGenerate].Get();
				compute_pso_desc.CS = shader_map[CS_BokehGenerate];
				BREAK_IF_FAILED(device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&pso_map[PSO::eBokehGenerate])));


				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pso_desc{};
				graphics_pso_desc.InputLayout = { nullptr, 0 };
				graphics_pso_desc.pRootSignature = rs_map[RootSig::eBokeh].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&graphics_pso_desc, IID_PPV_ARGS(&pso_map[PSO::eBokeh])));
			}

			//clouds
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eClouds].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eClouds])));
			}

			//motion blur
			{

				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
				pso_desc.InputLayout = { nullptr, 0 };
				pso_desc.pRootSignature = rs_map[RootSig::eMotionBlur].Get();
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

				BREAK_IF_FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_map[PSO::eMotionBlur])));
			}
		}

		
	}
	void Renderer::CreateDescriptorHeaps()
	{
		auto device = gfx->Device();

		rtv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 11));
		srv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 26));
		dsv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 11));
		uav_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 11));
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
	void Renderer::CreateViews(u32 width, u32 height)
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

			hdr_render_target = Texture2D(gfx->Device(), render_target_desc);
			hdr_render_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			hdr_render_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));

			render_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			prev_hdr_render_target = Texture2D(gfx->Device(), render_target_desc);
			prev_hdr_render_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			prev_hdr_render_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));

			sun_target = Texture2D(gfx->Device(), render_target_desc);
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

			depth_stencil_target = Texture2D(gfx->Device(), depth_target_desc);

			texture2d_srv_desc_t srv_desc{};
			srv_desc.format = DXGI_FORMAT_R32_FLOAT;
			depth_stencil_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++), &srv_desc);
			texture2d_dsv_desc_t dsv_desc{};
			dsv_desc.format = DXGI_FORMAT_D32_FLOAT;
			depth_stencil_target.CreateDSV(dsv_heap->GetCpuHandle(dsv_heap_index++), &dsv_desc);
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
			ldr_render_target = Texture2D(gfx->Device(), ldr_target_desc);

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

			for (u32 i = 0; i < GBUFFER_SIZE; ++i)
			{
				gbuffer.emplace_back(gfx->Device(), render_target_desc);

				gbuffer.back().CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
				gbuffer.back().CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			}

		}

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
				shadow_depth_map = Texture2D(gfx->Device(), depth_map_desc);

				texture2d_srv_desc_t srv_desc{};
				srv_desc.format = DXGI_FORMAT_R32_FLOAT;
				shadow_depth_map.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++), &srv_desc);

				texture2d_dsv_desc_t dsv_desc{};
				dsv_desc.format = DXGI_FORMAT_D32_FLOAT;
				shadow_depth_map.CreateDSV(dsv_heap->GetCpuHandle(dsv_heap_index++), &dsv_desc);
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

				shadow_depth_cubemap = TextureCube(gfx->Device(), depth_cubemap_desc);

				texturecube_srv_desc_t cube_srv_desc{};
				cube_srv_desc.format = DXGI_FORMAT_R32_FLOAT;
				shadow_depth_cubemap.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++), &cube_srv_desc);


				texturecube_dsv_desc_t cube_dsv_desc{};
				cube_dsv_desc.format = DXGI_FORMAT_D32_FLOAT;

				std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6> dsv_handles{};
				for (auto& dsv_handle : dsv_handles) dsv_handle = dsv_heap->GetCpuHandle(dsv_heap_index++);

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
				shadow_depth_cascades = Texture2DArray(gfx->Device(), depth_cascade_maps_desc);

				texture2darray_srv_desc_t array_srv_desc{};
				array_srv_desc.format = DXGI_FORMAT_R32_FLOAT;
				shadow_depth_cascades.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++), &array_srv_desc);

				texture2darray_dsv_desc_t array_dsv_desc{};
				array_dsv_desc.format = DXGI_FORMAT_D32_FLOAT;

				std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsv_handles(CASCADE_COUNT);
				for (auto& dsv_handle : dsv_handles) dsv_handle = dsv_heap->GetCpuHandle(dsv_heap_index++);

				shadow_depth_cascades.CreateDSVs(dsv_handles, &array_dsv_desc);
			}
		}

		//ssao
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

			ssao_texture = Texture2D(gfx->Device(), ssao_target_desc);

			ssao_texture.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			ssao_texture.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));

			//noise texture
			texture2d_desc_t noise_desc{};
			noise_desc.width = SSAO_NOISE_DIM;
			noise_desc.height = SSAO_NOISE_DIM;
			noise_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			noise_desc.start_state = D3D12_RESOURCE_STATE_COPY_DEST;
			noise_desc.clear_value.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

			noise_texture = Texture2D(gfx->Device(), noise_desc);


			RealRandomGenerator rand_float{ 0.0f, 1.0f };

			for (u32 i = 0; i < SSAO_KERNEL_SIZE; i++)
			{
				DirectX::XMFLOAT4 _offset = DirectX::XMFLOAT4(2 * rand_float() - 1, 2 * rand_float() - 1, rand_float(), 0.0f);
				DirectX::XMVECTOR offset = DirectX::XMLoadFloat4(&_offset);
				offset = DirectX::XMVector4Normalize(offset);

				offset *= rand_float();

				ssao_kernel[i] = offset;
			}
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

			blur_intermediate_texture = Texture2D(gfx->Device(), blur_desc);

			blur_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			blur_final_texture = Texture2D(gfx->Device(), blur_desc);

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
			bloom_extract_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			bloom_extract_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			bloom_extract_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; //t0 in CS
			bloom_extract_desc.clear_value.Format = bloom_extract_desc.format;

			bloom_extract_texture = Texture2D(gfx->Device(), bloom_extract_desc);

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
			render_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			render_target_desc.start_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

			postprocess_textures[0] = Texture2D(gfx->Device(), render_target_desc);
			postprocess_textures[0].CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			postprocess_textures[0].CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));

			render_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postprocess_textures[1] = Texture2D(gfx->Device(), render_target_desc);
			postprocess_textures[1].CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			postprocess_textures[1].CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
		}

		//tiled deferred
		{

			texture2d_desc_t uav_target_desc{};
			uav_target_desc.width = width;
			uav_target_desc.height = height;
			uav_target_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			uav_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			uav_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			uav_target = Texture2D(gfx->Device(), uav_target_desc);
			uav_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			uav_target.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));

			texture2d_desc_t tiled_debug_desc{};
			tiled_debug_desc.width = width;
			tiled_debug_desc.height = height;
			tiled_debug_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tiled_debug_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			tiled_debug_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			debug_tiled_texture = Texture2D(gfx->Device(), tiled_debug_desc);
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
			offscreen_ldr_target = Texture2D(gfx->Device(), offscreen_ldr_target_desc);

			offscreen_ldr_target.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			offscreen_ldr_target.CreateRTV(rtv_heap->GetCpuHandle(rtv_heap_index++));
		}

		//clustered deferred
		{

			clusters.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			clusters.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));

			light_counter.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			light_counter.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));

			light_list.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			light_list.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));

			light_grid.CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			light_grid.CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
		}

		//bokeh
		{
			bokeh = std::make_unique<StructuredBuffer<Bokeh>>(gfx->Device(), width * height, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			bokeh->CreateSRV(srv_heap->GetCpuHandle(srv_heap_index++));
			bokeh->CreateUAV(uav_heap->GetCpuHandle(uav_heap_index++));
			bokeh->CreateCounterUAV(uav_heap->GetCpuHandle(uav_heap_index++));
		}
		
	}
	void Renderer::CreateRenderPasses(u32 width, u32 height)
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
			dsv_attachment_desc.cpu_handle = depth_stencil_target.DSV();
			dsv_attachment_desc.clear_value.Format = DXGI_FORMAT_D32_FLOAT;
			dsv_attachment_desc.clear_value.DepthStencil.Depth = 1.0f;
			dsv_attachment_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			dsv_attachment_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.dsv_attachment = dsv_attachment_desc;

			render_pass_desc.width = width;
			render_pass_desc.height = height;
			gbuffer_render_pass = RenderPass(render_pass_desc);
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
			dsv_attachment_desc.cpu_handle = depth_stencil_target.DSV();
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
			for (u32 i = 0; i < shadow_cubemap_passes.size(); ++i)
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
			
			for (u32 i = 0; i < CASCADE_COUNT; ++i)
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
			rtv_attachment_desc_t ssao_attachment_desc{};
			ssao_attachment_desc.cpu_handle = ssao_texture.RTV();
			ssao_attachment_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			ssao_attachment_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			render_pass_desc.rtv_attachments.push_back(ssao_attachment_desc);
			render_pass_desc.width = width;
			render_pass_desc.height = height;
			
			ssao_render_pass = RenderPass(render_pass_desc);
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
	}
	void Renderer::CreateIBLTextures()
	{
		auto device = gfx->Device();
		auto cmd_list = gfx->DefaultCommandList();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		
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

			ResourceBarriers precopy_barriers{};
			precopy_barriers.AddTransition(env_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			precopy_barriers.AddTransition(unfiltered_env_resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			precopy_barriers.Submit(cmd_list);

			
			for (u32 array_slice = 0; array_slice < 6; ++array_slice)
			{
				const u32 subresource_index = D3D12CalcSubresource(0, array_slice, 0, env_desc.MipLevels, 6);
				const u32 unfiltered_subresource_index = D3D12CalcSubresource(0, array_slice, 0, unfiltered_env_desc.MipLevels, 6);
				auto dst_copy_region = CD3DX12_TEXTURE_COPY_LOCATION{ env_texture.Get(), subresource_index };
				auto src_copy_region = CD3DX12_TEXTURE_COPY_LOCATION{ unfiltered_env_resource, unfiltered_subresource_index };
				cmd_list->CopyTextureRegion(&dst_copy_region, 0, 0, 0, &src_copy_region, nullptr);
			}
			
			ResourceBarriers postcopy_barriers{};
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

			float delta_roughness = 1.0f / std::max<f32>(f32(env_desc.MipLevels - 1), 1.0f);
			for (u32 level = 1, size = std::max<u64>(unfiltered_env_desc.Width, unfiltered_env_desc.Height) / 2; level < env_desc.MipLevels; ++level, size /= 2)
			{
				const u32 num_groups = std::max<u32>(1, size / 32);
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

			gfx->ExecuteCommandList();
			gfx->WaitForGPU();
			gfx->ResetCommandList();
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
			cmd_list->SetDescriptorHeaps(_countof(pp_heaps), pp_heaps);

			device->CopyDescriptorsSimple(1,
				descriptor_allocator->GetCpuHandle(descriptor_index + 1), ibl_heap->GetCpuHandle(ENV_TEXTURE_SLOT),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index + 1));
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch(desc.Width / 32, desc.Height / 32, 6);
			irmap_barrier = CD3DX12_RESOURCE_BARRIER::Transition(irmap_texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			cmd_list->ResourceBarrier(1, &irmap_barrier);

			gfx->ExecuteCommandList();
			gfx->WaitForGPU();
			gfx->ResetCommandList();
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
			cmd_list->SetDescriptorHeaps(_countof(pp_heaps), pp_heaps);

			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
			cmd_list->Dispatch(desc.Width / 32, desc.Height / 32, 1);

			brdf_barrier = CD3DX12_RESOURCE_BARRIER::Transition(brdf_lut_texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			cmd_list->ResourceBarrier(1, &brdf_barrier);

			gfx->ExecuteCommandList();
			gfx->WaitForGPU();
			gfx->ResetCommandList();

			cmd_list->SetDescriptorHeaps(_countof(pp_heaps), pp_heaps);
		}

		ibl_textures_generated = true;
	}

	void Renderer::UpdateConstantBuffers(f32 dt)
	{
		//frame
		{
			
			frame_cbuf_data.global_ambient = XMVectorSet(settings.ambient_color[0], settings.ambient_color[1], settings.ambient_color[2], 1);
			frame_cbuf_data.camera_near = camera->Near();
			frame_cbuf_data.camera_far = camera->Far();
			frame_cbuf_data.camera_position = camera->Position();
			frame_cbuf_data.view = camera->View();
			frame_cbuf_data.projection = camera->Proj();
			frame_cbuf_data.view_projection = camera->ViewProj();
			frame_cbuf_data.inverse_view = DirectX::XMMatrixInverse(nullptr, camera->View());
			frame_cbuf_data.inverse_projection = DirectX::XMMatrixInverse(nullptr, camera->Proj());
			frame_cbuf_data.inverse_view_projection = DirectX::XMMatrixInverse(nullptr, camera->ViewProj());
			frame_cbuf_data.screen_resolution_x = width;
			frame_cbuf_data.screen_resolution_y = height;

			frame_cbuffer.Update(frame_cbuf_data, backbuffer_index);

			frame_cbuf_data.prev_view_projection = camera->ViewProj();
		}

		//postprocess
		{
			PostprocessCBuffer postprocess_cbuf_data{};
			postprocess_cbuf_data.tone_map_exposure = settings.tonemap_exposure;
			postprocess_cbuf_data.tone_map_operator = static_cast<int>(settings.tone_map_op);
			postprocess_cbuf_data.noise_scale = XMFLOAT2((f32)width / 8, (f32)height / 8);
			postprocess_cbuf_data.ssao_power = settings.ssao_power;
			postprocess_cbuf_data.ssao_radius = settings.ssao_radius;
			for (u32 i = 0; i < SSAO_KERNEL_SIZE; ++i) postprocess_cbuf_data.samples[i] = ssao_kernel[i];
			postprocess_cbuf_data.ssr_ray_step = settings.ssr_ray_step;
			postprocess_cbuf_data.ssr_ray_hit_threshold = settings.ssr_ray_hit_threshold;
			postprocess_cbuf_data.dof_params = XMVectorSet(settings.dof_near_blur, settings.dof_near, settings.dof_far, settings.dof_far_blur);
			postprocess_cbuf_data.motion_blur_intensity = settings.motion_blur_intensity;
			postprocess_cbuf_data.fog_falloff = settings.fog_falloff;
			postprocess_cbuf_data.fog_density = settings.fog_density;
			postprocess_cbuf_data.fog_type = static_cast<i32>(settings.fog_type);
			postprocess_cbuf_data.fog_start = settings.fog_start;
			postprocess_cbuf_data.fog_color = XMVectorSet(settings.fog_color[0], settings.fog_color[1], settings.fog_color[2], 1);
			postprocess_cbuffer.Update(postprocess_cbuf_data, backbuffer_index);
		}

		//compute 
		{
			std::array<f32, 9> coeffs = GaussKernel<4>(settings.blur_sigma);
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

			compute_cbuffer.Update(compute_cbuf_data, backbuffer_index);
		}

		//weather
		{
			WeatherCBuffer weather_cbuf_data{};
			static f32 total_time = 0.0f;
			total_time += dt;

			auto lights = reg.view<Light>();

			for (auto light : lights)
			{
				auto const& light_data = lights.get(light);

				if (light_data.type == LightType::eDirectional && light_data.active)
				{
					weather_cbuf_data.light_dir = -light_data.direction;
					weather_cbuf_data.light_color = light_data.color * light_data.energy;
					break;
				}
			}

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

			weather_cbuffer.Update(weather_cbuf_data, backbuffer_index);
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
	void Renderer::LightFrustumCulling(LightType type)
	{
		BoundingFrustum camera_frustum = camera->Frustum();

		auto visibility_view = reg.view<Visibility>();

		for (auto e : visibility_view)
		{
			auto& visibility = visibility_view.get(e);

			switch (type)
			{
			case LightType::eDirectional:
				visibility.light_visible = light_bounding_box.Intersects(visibility.aabb);
				break;
			case LightType::eSpot:
			case LightType::ePoint:
				visibility.light_visible = light_bounding_frustum.Intersects(visibility.aabb);
				break;
			default:
				ADRIA_ASSERT(false);
			}
		}
	}


	void Renderer::PassGBuffer(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();
		auto gbuffer_view = reg.view<Mesh, Transform, Material, Deferred, Visibility>();

		ResourceBarriers gbuffer_barriers{};
		for (auto& texture : gbuffer)
			texture.Transition(gbuffer_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		gbuffer_barriers.Submit(cmd_list);

		gbuffer_render_pass.Begin(cmd_list);

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eGbufferPBR].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eGbufferPBR].Get());
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

			material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			material_allocation.Update(material_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles{};
			std::vector<u32> src_range_sizes{};

			ADRIA_ASSERT(material.albedo_texture != INVALID_TEXTURE_HANDLE);
			
			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.albedo_texture));
			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.metallic_roughness_texture));
			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.normal_texture));
			texture_handles.push_back(texture_manager.CpuDescriptorHandle(material.emissive_texture));

			src_range_sizes.push_back(1u);
			src_range_sizes.push_back(1u);
			src_range_sizes.push_back(1u);
			src_range_sizes.push_back(1u);


			OffsetType descriptor_index = descriptor_allocator->AllocateRange(texture_handles.size());
			auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			u32 dst_range_sizes[] = { (u32)texture_handles.size() };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, (u32)texture_handles.size(), texture_handles.data(), src_range_sizes.data(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

			mesh.Draw(cmd_list);
		}

		gbuffer_render_pass.End(cmd_list);

		gbuffer_barriers.ReverseTransitions();
		gbuffer_barriers.Submit(cmd_list);

	}
	void Renderer::PassSSAO(ID3D12GraphicsCommandList4* cmd_list)
	{

		ResourceBarriers ssao_barrier{};
		ssao_texture.Transition(ssao_barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);
		ssao_barrier.Submit(cmd_list);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		ssao_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eSSAO].Get());
			cmd_list->SetPipelineState(pso_map[PSO::eSSAO].Get());

			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
			cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);
			D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0].SRV(), depth_stencil_target.SRV(), noise_texture.SRV() };
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

		BlurTexture(cmd_list, ssao_texture);
	}
	void Renderer::PassAmbient(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		if (!ibl_textures_generated) settings.ibl = false;

		ambient_render_pass.Begin(cmd_list);

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eAmbientPBR].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

		
		if (settings.ssao && settings.ibl)cmd_list->SetPipelineState(pso_map[PSO::eAmbientPBR_SSAO_IBL].Get());
		else if (settings.ssao && !settings.ibl) cmd_list->SetPipelineState(pso_map[PSO::eAmbientPBR_SSAO].Get());
		else if (!settings.ssao && settings.ibl) 
			cmd_list->SetPipelineState(pso_map[PSO::eAmbientPBR_IBL].Get());
		else cmd_list->SetPipelineState(pso_map[PSO::eAmbientPBR].Get());

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = {gbuffer[0].SRV(), gbuffer[1].SRV(), gbuffer[2].SRV(), depth_stencil_target.SRV()};
		u32 src_range_sizes[] = {1,1,1,1};
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
		auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		u32 dst_range_sizes[] = { (u32)_countof(cpu_handles) };
		device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));


		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { null_srv_heap->GetCpuHandle(TEXTURE2D_SLOT), 
		null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT), null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT), null_srv_heap->GetCpuHandle(TEXTURE2D_SLOT) };
		u32 src_range_sizes2[] = { 1,1,1,1 };
		if (settings.ssao) cpu_handles2[0] = blur_final_texture.SRV(); //contains blurred ssao
		if (settings.ibl)
		{
			cpu_handles2[1] = ibl_heap->GetCpuHandle(ENV_TEXTURE_SLOT);
			cpu_handles2[2] = ibl_heap->GetCpuHandle(IRMAP_TEXTURE_SLOT);
			cpu_handles2[3] = ibl_heap->GetCpuHandle(BRDF_LUT_TEXTURE_SLOT);
		}

		descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles2));
		dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		u32 dst_range_sizes2[] = { (u32)_countof(cpu_handles2) };
		device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes2, _countof(cpu_handles2), cpu_handles2, src_range_sizes2,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

		ambient_render_pass.End(cmd_list);
	}
	void Renderer::PassDeferredLighting(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto upload_buffer = gfx->UploadBuffer();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		
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
			light_cbuf_data.type = static_cast<i32>(light_data.type);
			light_cbuf_data.use_cascades = light_data.use_cascades;
			light_cbuf_data.volumetric = light_data.volumetric;
			light_cbuf_data.volumetric_strength = light_data.volumetric_strength;
			light_cbuf_data.screenspace_shadows = light_data.screenspace_shadows;

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
				case LightType::eDirectional:
					if (light_data.use_cascades) PassShadowMapCascades(cmd_list, light_data);
					else PassShadowMapDirectional(cmd_list, light_data);
					break;
				case LightType::eSpot:
					PassShadowMapSpot(cmd_list, light_data);
					break;
				case LightType::ePoint:
					PassShadowMapPoint(cmd_list, light_data);
					break;
				default:
					ADRIA_ASSERT(false);
				}
			
			}

			//lighting + volumetric fog
			lighting_render_pass.Begin(cmd_list);
			{
				cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eLightingPBR].Get());
				cmd_list->SetPipelineState(pso_map[PSO::eLightingPBR].Get());

				cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

				cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);

				cmd_list->SetGraphicsRootConstantBufferView(2, shadow_allocation.gpu_address);

				//t0,t1,t2 - gbuffer and depth
				{
					D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0].SRV(), gbuffer[1].SRV(), depth_stencil_target.SRV() };
					u32 src_range_sizes[] = { 1,1,1 };

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
					D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
					u32 dst_range_sizes[] = { (u32)_countof(cpu_handles) };
					device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));
				}

				//t4,t5,t6 - shadow maps
				{
					D3D12_CPU_DESCRIPTOR_HANDLE shadow_cpu_handles[] = { null_srv_heap->GetCpuHandle(TEXTURE2D_SLOT),
						null_srv_heap->GetCpuHandle(TEXTURECUBE_SLOT), null_srv_heap->GetCpuHandle(TEXTURE2DARRAY_SLOT) }; 
					u32 src_range_sizes[] = { 1,1,1 };

					switch (light_data.type)
					{
					case LightType::eDirectional:
						if (light_data.use_cascades) shadow_cpu_handles[TEXTURE2DARRAY_SLOT] = shadow_depth_cascades.SRV();
						else shadow_cpu_handles[TEXTURE2D_SLOT] = shadow_depth_map.SRV();
						break;
					case LightType::eSpot:
						shadow_cpu_handles[TEXTURE2D_SLOT] = shadow_depth_map.SRV();
						break;
					case LightType::ePoint:
						shadow_cpu_handles[TEXTURECUBE_SLOT] = shadow_depth_cubemap.SRV();
						break;
					default:
						ADRIA_ASSERT(false);
					}

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(shadow_cpu_handles));
					auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
					u32 dst_range_sizes[] = { (u32)_countof(shadow_cpu_handles) };
					device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(shadow_cpu_handles), shadow_cpu_handles, src_range_sizes,
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

		auto device = gfx->Device();
		auto upload_buffer = gfx->UploadBuffer();
		auto descriptor_allocator = gfx->DescriptorAllocator();

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


		ResourceBarriers tiled_barriers{};
		depth_stencil_target.Transition(tiled_barriers, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gbuffer[0].Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gbuffer[1].Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		uav_target.Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		debug_tiled_texture.Transition(tiled_barriers, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		tiled_barriers.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(rs_map[RootSig::eTiledLighting].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eTiledLighting].Get());

		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(1, compute_cbuffer.View(backbuffer_index).BufferLocation);

		//t0,t1,t2 - gbuffer and depth
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0].SRV(), gbuffer[1].SRV(), depth_stencil_target.SRV() };
			u32 src_range_sizes[] = { 1,1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			u32 dst_range_sizes[] = { (u32)_countof(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		}

		//u0 i u1
		D3D12_GPU_DESCRIPTOR_HANDLE uav_target_for_clear{};
		D3D12_GPU_DESCRIPTOR_HANDLE uav_debug_for_clear{};
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { uav_target.UAV(), debug_tiled_texture.UAV() };
			u32 src_range_sizes[] = { 1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			u32 dst_range_sizes[] = { (u32)_countof(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

			uav_target_for_clear = descriptor_allocator->GetGpuHandle(descriptor_index);
			uav_debug_for_clear = descriptor_allocator->GetGpuHandle(descriptor_index + 1);
		}

		{
			u64 alloc_size = sizeof(StructuredLight) * structured_lights.size();
			DynamicAllocation dynamic_alloc = upload_buffer->Allocate(alloc_size, sizeof(StructuredLight));
			dynamic_alloc.Update(structured_lights.data(), alloc_size);

			auto i = descriptor_allocator->Allocate();

			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = dynamic_alloc.size / desc.Buffer.StructureByteStride;
			desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;

			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, descriptor_allocator->GetCpuHandle(i));

			cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(i));

		}

		f32 black[4] = { 0.0f,0.0f,0.0f,0.0f };

		cmd_list->ClearUnorderedAccessViewFloat(uav_target_for_clear, uav_target.UAV(), uav_target.Resource(),
			black, 0, nullptr);
		cmd_list->ClearUnorderedAccessViewFloat(uav_debug_for_clear, debug_tiled_texture.UAV(), debug_tiled_texture.Resource(),
			black, 0, nullptr);

		cmd_list->Dispatch((u32)std::ceil(width * 1.0f / 16), (height * 1.0f / 16), 1);


		tiled_barriers.ReverseTransitions();
		tiled_barriers.Submit(cmd_list);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = hdr_render_target.RTV();
		cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		if (settings.visualize_tiled) AddTextures(cmd_list, uav_target, debug_tiled_texture, BlendMode::eAlphaBlend);
		else CopyTexture(cmd_list, uav_target, BlendMode::eAdditiveBlend);


	}
	void Renderer::PassDeferredClusteredLighting(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.use_clustered_deferred);

		auto device = gfx->Device();
		auto upload_buffer = gfx->UploadBuffer();
		auto descriptor_allocator = gfx->DescriptorAllocator();

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
		u64 alloc_size = sizeof(StructuredLight) * structured_lights.size();
		DynamicAllocation dynamic_alloc = upload_buffer->Allocate(alloc_size, sizeof(StructuredLight));
		dynamic_alloc.Update(structured_lights.data(), alloc_size);

		//cluster building
		if (recreate_clusters)
		{

			cmd_list->SetComputeRootSignature(rs_map[RootSig::eClusterBuilding].Get());
			cmd_list->SetPipelineState(pso_map[PSO::eClusterBuilding].Get());
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
			cmd_list->SetComputeRootSignature(rs_map[RootSig::eClusterCulling].Get());
			cmd_list->SetPipelineState(pso_map[PSO::eClusterCulling].Get());
		
			OffsetType i = descriptor_allocator->AllocateRange(2);
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(i);
			device->CopyDescriptorsSimple(1, dst_descriptor, clusters.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = dynamic_alloc.size / desc.Buffer.StructureByteStride;
			desc.Buffer.FirstElement = dynamic_alloc.offset / desc.Buffer.StructureByteStride;
			device->CreateShaderResourceView(dynamic_alloc.buffer, &desc, descriptor_allocator->GetCpuHandle(i + 1));
		
			cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(i));
		
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { light_counter.UAV(), light_list.UAV(), light_grid.UAV() };
			u32 src_range_sizes[] = { 1,1,1 };
			i = descriptor_allocator->AllocateRange(_countof(cpu_handles));
			dst_descriptor = descriptor_allocator->GetCpuHandle(i);
			u32 dst_range_sizes[] = { (u32)_countof(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(i));
			cmd_list->Dispatch(CLUSTER_SIZE_X / 16, CLUSTER_SIZE_Y / 16, CLUSTER_SIZE_Z / 1);
		}


		//clustered lighting
		lighting_render_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eClusteredLightingPBR].Get());
			cmd_list->SetPipelineState(pso_map[PSO::eClusteredLightingPBR].Get());
			cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

			//gbuffer
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { gbuffer[0].SRV(), gbuffer[1].SRV(), depth_stencil_target.SRV() };
			u32 src_range_sizes[] = { 1,1,1 };
			OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			u32 dst_range_sizes[] = { (u32)_countof(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

			//light stuff
			descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles) + 1);
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles2[] = { light_list.SRV(), light_grid.SRV() };
			u32 src_range_sizes2[] = { 1,1 };

			dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index + 1);
			u32 dst_range_sizes2[] = { (u32)_countof(cpu_handles2) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes2, _countof(cpu_handles2), cpu_handles2, src_range_sizes2,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
			desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			desc.Buffer.StructureByteStride = sizeof(StructuredLight);
			desc.Buffer.NumElements = dynamic_alloc.size / desc.Buffer.StructureByteStride;
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
		forward_render_pass.Begin(cmd_list);
		PassForwardCommon(cmd_list, false);
		PassSkybox(cmd_list);
		PassForwardCommon(cmd_list, true);
		forward_render_pass.End(cmd_list);
	}
	void Renderer::PassPostprocess(ID3D12GraphicsCommandList4* cmd_list)
	{
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

		ResourceBarriers postprocess_barriers{};
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

			ResourceBarriers cloud_barriers{};
			postprocess_textures[!postprocess_index].Transition(cloud_barriers,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			cloud_barriers.Submit(cmd_list);

			BlurTexture(cmd_list, postprocess_textures[!postprocess_index]);

			cloud_barriers.ReverseTransitions();
			cloud_barriers.Submit(cmd_list);

			postprocess_passes[postprocess_index].Begin(cmd_list);
			CopyTexture(cmd_list, blur_final_texture, BlendMode::eAlphaBlend);
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
			ResourceBarriers barrier{};
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

			postprocess_passes[postprocess_index].Begin(cmd_list);

			AddTextures(cmd_list, postprocess_textures[!postprocess_index], blur_final_texture);

			postprocess_passes[postprocess_index].End(cmd_list);

			postprocess_barriers.ReverseTransitions();
			postprocess_barriers.Submit(cmd_list);
			postprocess_index = !postprocess_index;
		}
		
		for (entity light : lights)
		{
			auto const& light_data = lights.get(light);
			if (!light_data.active) continue;

			if (light_data.type == LightType::eDirectional)
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

			ResourceBarriers taa_barrier{};
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
		ADRIA_ASSERT(light.type == LightType::eDirectional);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		
		auto const& [V, P] = scene_bounding_sphere ? LightViewProjection_Directional(light, *scene_bounding_sphere, light_bounding_box)
			: LightViewProjection_Directional(light, *camera, light_bounding_box);
		shadow_cbuf_data.lightview = V;
		shadow_cbuf_data.lightviewprojection = V * P;
		shadow_cbuf_data.shadow_map_size = SHADOW_MAP_SIZE;
		shadow_cbuf_data.softness = settings.shadow_softness;
		shadow_cbuf_data.shadow_matrix1 = XMMatrixInverse(nullptr, camera->View()) * shadow_cbuf_data.lightviewprojection;

		shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		shadow_allocation.Update(shadow_cbuf_data);


		ResourceBarriers shadow_map_barrier{};
		shadow_depth_map.Transition(shadow_map_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_map_barrier.Submit(cmd_list);


		shadow_map_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eDepthMap].Get());
			cmd_list->SetPipelineState(pso_map[PSO::eDepthMap].Get());
			cmd_list->SetGraphicsRootConstantBufferView(1, shadow_allocation.gpu_address);


			LightFrustumCulling(LightType::eDirectional);

			auto shadow_view = reg.view<Mesh, Transform, Visibility>();
			for (auto e : shadow_view)
			{
				auto const& visibility = shadow_view.get<Visibility>(e); 
				if (visibility.light_visible)
				{
					auto const& transform = shadow_view.get<Transform>(e); //add const
					auto const& mesh = shadow_view.get<Mesh>(e);

					object_cbuf_data.model = transform.current_transform;
					object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

					object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					object_allocation.Update(object_cbuf_data);
					cmd_list->SetGraphicsRootConstantBufferView(0, object_allocation.gpu_address);

					mesh.Draw(cmd_list);
				}
			}

			//add transparent part later
		}
		shadow_map_pass.End(cmd_list);
		
		shadow_map_barrier.ReverseTransitions();
		shadow_map_barrier.Submit(cmd_list);
	}
	void Renderer::PassShadowMapSpot(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == LightType::eSpot);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		
		//update cbuffer
		auto const& [V, P] = scene_bounding_sphere ? LightViewProjection_Directional(light, *scene_bounding_sphere, light_bounding_box)
			: LightViewProjection_Directional(light, *camera, light_bounding_box);
		shadow_cbuf_data.lightview = V;
		shadow_cbuf_data.lightviewprojection = V * P;
		shadow_cbuf_data.shadow_map_size = SHADOW_MAP_SIZE;
		shadow_cbuf_data.softness = settings.shadow_softness;
		shadow_cbuf_data.shadow_matrix1 = XMMatrixInverse(nullptr, camera->View()) * shadow_cbuf_data.lightviewprojection;

		shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		shadow_allocation.Update(shadow_cbuf_data);

		
		ResourceBarriers shadow_map_barrier{};
		shadow_depth_map.Transition(shadow_map_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_map_barrier.Submit(cmd_list);


		shadow_map_pass.Begin(cmd_list);
		{
			cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eDepthMap].Get());
			cmd_list->SetPipelineState(pso_map[PSO::eDepthMap].Get());
			cmd_list->SetGraphicsRootConstantBufferView(1, shadow_allocation.gpu_address);


			LightFrustumCulling(LightType::eDirectional);

			auto shadow_view = reg.view<Mesh, Transform, Visibility>();
			for (auto e : shadow_view)
			{
				auto const& visibility = shadow_view.get<Visibility>(e);
				if (visibility.light_visible)
				{
					auto const& transform = shadow_view.get<Transform>(e); //add const
					auto const& mesh = shadow_view.get<Mesh>(e);

					object_cbuf_data.model = transform.current_transform;
					object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

					object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					object_allocation.Update(object_cbuf_data);
					cmd_list->SetGraphicsRootConstantBufferView(0, object_allocation.gpu_address);

					mesh.Draw(cmd_list);
				}
			}

			//add transparent part later
		}
		shadow_map_pass.End(cmd_list);

		shadow_map_barrier.ReverseTransitions();
		shadow_map_barrier.Submit(cmd_list);
	}
	void Renderer::PassShadowMapPoint(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == LightType::ePoint);
		
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		
		ResourceBarriers shadow_cubemap_barrier{};
		shadow_depth_cubemap.Transition(shadow_cubemap_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_cubemap_barrier.Submit(cmd_list);


		for (u32 i = 0; i < shadow_cubemap_passes.size(); ++i)
		{
			//Update CBuffers
			
			auto const& [V, P] = LightViewProjection_Point(light, i, light_bounding_frustum);
			shadow_cbuf_data.lightviewprojection = V * P;
			shadow_cbuf_data.lightview = V;
				
			shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

			shadow_allocation.Update(shadow_cbuf_data);

			
			shadow_cubemap_passes[i].Begin(cmd_list);
			{
				
				cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eDepthMap].Get());
				cmd_list->SetPipelineState(pso_map[PSO::eDepthMap].Get());
				cmd_list->SetGraphicsRootConstantBufferView(1, shadow_allocation.gpu_address);

				LightFrustumCulling(LightType::ePoint);

				auto shadow_view = reg.view<Mesh, Transform, Visibility>();

				for (auto e : shadow_view)
				{
					auto& visibility = shadow_view.get<Visibility>(e);
					if (visibility.light_visible)
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
				}
				
			}
			shadow_cubemap_passes[i].End(cmd_list);
		}

		
		shadow_cubemap_barrier.ReverseTransitions();
		shadow_cubemap_barrier.Submit(cmd_list);

	}
	void Renderer::PassShadowMapCascades(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.type == LightType::eDirectional);
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		ResourceBarriers shadow_cascades_barrier{};
		shadow_depth_cascades.Transition(shadow_cascades_barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		shadow_cascades_barrier.Submit(cmd_list);

		std::array<f32, CASCADE_COUNT> split_distances;
		std::array<XMMATRIX, CASCADE_COUNT> proj_matrices = RecalculateProjectionMatrices(*camera, settings.split_lambda, split_distances);
		std::array<XMMATRIX, CASCADE_COUNT> light_view_projections{};

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eDepthMap].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eDepthMap].Get());
		
		for (u32 i = 0; i < CASCADE_COUNT; ++i)
		{
			//Update
			
			auto const& [V, P] = LightViewProjection_Cascades(light, *camera, proj_matrices[i], light_bounding_box);
			light_view_projections[i] = V * P;
			shadow_cbuf_data.lightview = V;
			shadow_cbuf_data.lightviewprojection = light_view_projections[i];
			shadow_cbuf_data.softness = settings.shadow_softness;

			shadow_allocation = upload_buffer->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			shadow_allocation.Update(shadow_cbuf_data);

			cmd_list->SetGraphicsRootConstantBufferView(1, shadow_allocation.gpu_address);
			
			shadow_cascades_passes[i].Begin(cmd_list);
			{
				LightFrustumCulling(LightType::eDirectional);
				auto shadow_view = reg.view<Mesh, Transform, Visibility>();
				for (auto e : shadow_view)
				{
					auto& visibility = shadow_view.get<Visibility>(e); 
					if (visibility.light_visible)
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
				}
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
	void Renderer::PassVolumetric(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.volumetric);
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		if (light.type == LightType::eDirectional && !light.casts_shadows)
		{
			Log::Warning("Calling PassVolumetric on a Directional Light \
				that does not cast shadows does not make sense!\n");
			return;
		}
		if (!settings.fog)
		{
			Log::Warning("Volumetric Lighting requires Fog to be enabled!\n");
			return;
		}

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eVolumetric].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, light_allocation.gpu_address);
		cmd_list->SetGraphicsRootConstantBufferView(2, shadow_allocation.gpu_address);
		cmd_list->SetGraphicsRootConstantBufferView(3, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { depth_stencil_target.SRV(), {} };
		u32 src_range_sizes[] = { 1,1 };

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
		D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		u32 dst_range_sizes[] = { (u32)_countof(cpu_handles) };

		switch (light.type)
		{
		case LightType::eDirectional:
			if (light.use_cascades)
			{
				cmd_list->SetPipelineState(pso_map[PSO::eVolumetric_DirectionalCascades].Get());
				cpu_handles[1] = shadow_depth_cascades.SRV();
			}
			else
			{
				cmd_list->SetPipelineState(pso_map[PSO::eVolumetric_Directional].Get());
				cpu_handles[1] = shadow_depth_map.SRV();
			}
			break;
		case LightType::eSpot:
			cmd_list->SetPipelineState(pso_map[PSO::eVolumetric_Spot].Get());
			cpu_handles[1] = shadow_depth_map.SRV();
			break;
		case LightType::ePoint:
			cmd_list->SetPipelineState(pso_map[PSO::eVolumetric_Point].Get());
			cpu_handles[1] = shadow_depth_cubemap.SRV();
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Light Type!");
		}

		device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

	}
	void Renderer::PassForwardCommon(ID3D12GraphicsCommandList4* cmd_list, bool transparent)
	{
		auto forward_view = reg.view<Mesh, Transform, Material, Forward, Visibility>();

		std::unordered_map<PSO, std::vector<entity>> entities_group;

		for (auto e : forward_view)
		{
			auto const& material = reg.get<Material>(e);
			auto const& forward = reg.get<Forward>(e);

			if (forward.transparent == transparent)
				entities_group[material.pso].push_back(e);
		}

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		if (entities_group.empty()) return;

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eForward].Get());
		
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
				
				material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				material_allocation.Update(material_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

				D3D12_CPU_DESCRIPTOR_HANDLE diffuse_handle = texture_manager.CpuDescriptorHandle(material.diffuse_texture);
				u32 src_range_size = 1;

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);

				device->CopyDescriptorsSimple(1, dst_descriptor, diffuse_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetGraphicsRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

				mesh.Draw(cmd_list);
			}

		}

	}
	void Renderer::PassSkybox(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eSkybox].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eSkybox].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);

		ObjectCBuffer object_cbuf_data{};
		object_cbuf_data.model = DirectX::XMMatrixTranslationFromVector(camera->Position());

		object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		object_allocation.Update(object_cbuf_data);
		cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

		auto skybox_view = reg.view<Mesh, Transform, Skybox>();

		for (auto e : skybox_view)
		{
			auto const& [transformation, mesh, skybox] = skybox_view.get<Transform, Mesh, Skybox>(e);

			if (!skybox.active) continue;

			OffsetType descriptor_index = descriptor_allocator->Allocate();
			ADRIA_ASSERT(skybox.cubemap_texture != INVALID_TEXTURE_HANDLE);
			D3D12_CPU_DESCRIPTOR_HANDLE texture_handle = texture_manager.CpuDescriptorHandle(skybox.cubemap_texture);

			device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), texture_handle,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			cmd_list->SetGraphicsRootDescriptorTable(2,
				descriptor_allocator->GetGpuHandle(descriptor_index));

			mesh.Draw(cmd_list);

		}
	}

	void Renderer::PassLensFlare(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.lens_flare);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		lens_flare_textures.push_back(depth_stencil_target.SRV());

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eLensFlare].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eLensFlare].Get());

		light_allocation = upload_buffer->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		
		{
			auto camera_position = camera->Position();
			XMVECTOR light_position = light.type == LightType::eDirectional ?
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
		u32 src_range_sizes[] = { 1, 1, 1, 1, 1, 1, 1, 1 };
		u32 dst_range_sizes[] = { 8 };
		device->CopyDescriptors(_countof(dst_ranges), dst_ranges, dst_range_sizes, _countof(src_range_sizes), lens_flare_textures.data(), src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		cmd_list->DrawInstanced(7, 1, 0, 0);

		lens_flare_textures.pop_back();
	}
	void Renderer::PassVolumetricClouds(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		
		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eClouds].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eClouds].Get());
		
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, weather_cbuffer.View(backbuffer_index).BufferLocation);
		
		OffsetType descriptor_index = descriptor_allocator->AllocateRange(4);
		
		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { clouds_textures[0], clouds_textures[1], clouds_textures[2], depth_stencil_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		u32 src_range_sizes[] = { 1, 1, 1, 1 };
		u32 dst_range_sizes[] = { 4 };
		
		
		device->CopyDescriptors(_countof(dst_ranges), dst_ranges, dst_range_sizes, _countof(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		
		cmd_list->DrawInstanced(4, 1, 0, 0);


	}
	void Renderer::PassSSR(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eSSR].Get());

		cmd_list->SetPipelineState(pso_map[PSO::eSSR].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { gbuffer[0].SRV(), postprocess_textures[!postprocess_index].SRV(), depth_stencil_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		u32 src_range_sizes[] = { 1, 1, 1 };
		u32 dst_range_sizes[] = { 3 };

		
		device->CopyDescriptors(_countof(dst_ranges), dst_ranges, dst_range_sizes, _countof(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassDepthOfField(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eDof].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eDof].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index].SRV(), blur_final_texture.SRV(), depth_stencil_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		u32 src_range_sizes[] = { 1, 1, 1 };
		u32 dst_range_sizes[] = { 3 };

		device->CopyDescriptors(_countof(dst_ranges), dst_ranges, dst_range_sizes, _countof(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);

		if (settings.bokeh) PassDrawBokeh(cmd_list);

	}
	void Renderer::PassGenerateBokeh(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof && settings.bokeh);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		D3D12_RESOURCE_BARRIER prereset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,  D3D12_RESOURCE_STATE_COPY_DEST);
		cmd_list->ResourceBarrier(1, &prereset_barrier);
		cmd_list->CopyBufferRegion(bokeh->CounterBuffer(), 0, counter_reset_buffer.Get(), 0, sizeof(u32));
		D3D12_RESOURCE_BARRIER postreset_barrier = CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(),D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmd_list->ResourceBarrier(1, &postreset_barrier);

 		cmd_list->SetComputeRootSignature(rs_map[RootSig::eBokehGenerate].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eBokehGenerate].Get());
		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetComputeRootConstantBufferView(2, compute_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

		D3D12_RESOURCE_BARRIER dispatch_barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(bokeh->Buffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		cmd_list->ResourceBarrier(_countof(dispatch_barriers), dispatch_barriers);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index].SRV(), depth_stencil_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		u32 src_range_sizes[] = { 1, 1 };
		u32 dst_range_sizes[] = { 2 };
		device->CopyDescriptors(_countof(dst_ranges), dst_ranges, dst_range_sizes, _countof(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

		D3D12_CPU_DESCRIPTOR_HANDLE bokeh_uav = bokeh->UAV();
		descriptor_index = descriptor_allocator->Allocate();

		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), bokeh_uav,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->Dispatch((u32)std::ceil(width / 32.0f), (u32)std::ceil(height / 32.0f), 1);

		CD3DX12_RESOURCE_BARRIER precopy_barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh->Buffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_target.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)
		};
		cmd_list->ResourceBarrier(_countof(precopy_barriers), precopy_barriers);

		cmd_list->CopyBufferRegion(bokeh_indirect_draw_buffer.Get(), 0, bokeh->CounterBuffer(), 0, bokeh->CounterBuffer()->GetDesc().Width);

		CD3DX12_RESOURCE_BARRIER postcopy_barriers[] = 
		{
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh_indirect_draw_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
				CD3DX12_RESOURCE_BARRIER::Transition(bokeh->CounterBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		cmd_list->ResourceBarrier(_countof(postcopy_barriers), postcopy_barriers);
	}
	void Renderer::PassDrawBokeh(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.dof && settings.bokeh);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		
		D3D12_CPU_DESCRIPTOR_HANDLE bokeh_descriptor{};
		switch (settings.bokeh_type)
		{
		case BokehType::eHex:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(hex_bokeh_handle);
			break;
		case BokehType::eOct:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(oct_bokeh_handle);
			break;
		case BokehType::eCircle:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(circle_bokeh_handle);
			break;
		case BokehType::eCross:
			bokeh_descriptor = texture_manager.CpuDescriptorHandle(cross_bokeh_handle);
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Bokeh Type");
		}

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eBokeh].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eBokeh].Get());

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
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		
		ResourceBarriers barrier{};
		bloom_extract_texture.Transition(barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		postprocess_textures[!postprocess_index].Transition(barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		barrier.Submit(cmd_list);
		
		cmd_list->SetComputeRootSignature(rs_map[RootSig::eBloomExtract].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eBloomExtract].Get());
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
		
		cmd_list->Dispatch((u32)std::ceil(postprocess_textures[!postprocess_index].Width() / 32), 
						   (u32)std::ceil(postprocess_textures[!postprocess_index].Height() / 32), 1);
		
		barrier.ReverseTransitions();
		barrier.Submit(cmd_list);

		BlurTexture(cmd_list, bloom_extract_texture);
		
	}
	void Renderer::PassMotionBlur(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eMotionBlur].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eMotionBlur].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { postprocess_textures[!postprocess_index].SRV(), depth_stencil_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		u32 src_range_sizes[] = { 1, 1 };
		u32 dst_range_sizes[] = { 2 };

		device->CopyDescriptors(_countof(dst_ranges), dst_ranges, dst_range_sizes, _countof(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassFog(ID3D12GraphicsCommandList4* cmd_list)
	{
		ADRIA_ASSERT(settings.fog);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eFog].Get());

		cmd_list->SetPipelineState(pso_map[PSO::eFog].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		cmd_list->SetGraphicsRootConstantBufferView(1, postprocess_cbuffer.View(backbuffer_index).BufferLocation);

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(3);

		D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = {postprocess_textures[!postprocess_index].SRV(), depth_stencil_target.SRV() };
		D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { descriptor_allocator->GetCpuHandle(descriptor_index) };
		u32 src_range_sizes[] = { 1, 1 };
		u32 dst_range_sizes[] = { 2 };

		device->CopyDescriptors(_countof(dst_ranges), dst_ranges, dst_range_sizes, _countof(src_ranges), src_ranges, src_range_sizes,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	
	void Renderer::PassGodRays(ID3D12GraphicsCommandList4* cmd_list, Light const& light)
	{
		ADRIA_ASSERT(light.god_rays);

		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		if (light.type != LightType::eDirectional)
		{
			Log::Warning("Using God Rays on a Non-Directional Light Source\n");
			return;
		}

		
		LightCBuffer light_cbuf_data{};
		{
			light_cbuf_data.godrays_decay = light.godrays_decay;
			light_cbuf_data.godrays_density = light.godrays_density;
			light_cbuf_data.godrays_exposure = light.godrays_exposure;
			light_cbuf_data.godrays_weight = light.godrays_weight;

			auto camera_position = camera->Position();
			XMVECTOR light_position = light.type == LightType::eDirectional ?
				XMVector4Transform(light.position, XMMatrixTranslation(XMVectorGetX(camera_position), 0.0f, XMVectorGetY(camera_position)))
				: light.position;

			DirectX::XMVECTOR light_pos_h = XMVector4Transform(light_position, camera->ViewProj());
			DirectX::XMFLOAT4 light_pos{};
			DirectX::XMStoreFloat4(&light_pos, light_pos_h);
			light_cbuf_data.ss_position = XMVectorSet(0.5f * light_pos.x / light_pos.w + 0.5f, -0.5f * light_pos.y / light_pos.w + 0.5f, light_pos.z / light_pos.w, 1.0f);

			static const f32 f_max_sun_dist = 1.3f;
			f32 f_max_dist = (std::max)(abs(XMVectorGetX(light_cbuf_data.ss_position)), abs(XMVectorGetY(light_cbuf_data.ss_position)));
			if (f_max_dist >= 1.0f)
				light_cbuf_data.color = XMVector3Transform(light.color, XMMatrixScaling((f_max_sun_dist - f_max_dist), (f_max_sun_dist - f_max_dist), (f_max_sun_dist - f_max_dist)));
			else light_cbuf_data.color = light.color;
		}
		
		light_allocation = upload_buffer->Allocate(GetCBufferSize<LightCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		light_allocation.Update(light_cbuf_data);

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eGodRays].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eGodRays].Get());

		cmd_list->SetGraphicsRootConstantBufferView(0, light_allocation.gpu_address);

		OffsetType descriptor_index = descriptor_allocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor = sun_target.SRV();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), cpu_descriptor,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		cmd_list->DrawInstanced(4, 1, 0, 0);
	}
	void Renderer::PassToneMap(ID3D12GraphicsCommandList4* cmd_list)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eToneMap].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eToneMap].Get());
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
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		ResourceBarriers fxaa_barrier{};
		ldr_render_target.Transition(fxaa_barrier, D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		fxaa_barrier.Submit(cmd_list);

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eFXAA].Get());

		cmd_list->SetPipelineState(pso_map[PSO::eFXAA].Get());

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
		
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();


		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eTAA].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eTAA].Get());

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index), postprocess_textures[!postprocess_index].SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetCpuHandle(descriptor_index + 1), prev_hdr_render_target.SRV(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		cmd_list->SetGraphicsRootDescriptorTable(0, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmd_list->DrawInstanced(4, 1, 0, 0);
	}

	void Renderer::DrawSun(ID3D12GraphicsCommandList4* cmd_list, tecs::entity sun)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();
		auto upload_buffer = gfx->UploadBuffer();

		ResourceBarriers sun_barrier{};
		sun_barrier.AddTransition(sun_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		sun_barrier.AddTransition(depth_stencil_target.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		sun_barrier.Submit(cmd_list);


		D3D12_CPU_DESCRIPTOR_HANDLE rtv = sun_target.RTV();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = depth_stencil_target.DSV();
		f32 black[4] = { 0.0f };
		cmd_list->ClearRenderTargetView(rtv, black, 0, nullptr);
		cmd_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eForward].Get());
		cmd_list->SetPipelineState(pso_map[PSO::eSun].Get());
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_cbuffer.View(backbuffer_index).BufferLocation);
		{
			auto [transform, mesh, material] = reg.get<Transform, Mesh, Material>(sun);

			object_cbuf_data.model = transform.current_transform;
			object_cbuf_data.inverse_transposed_model = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, object_cbuf_data.model));

			object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			object_allocation.Update(object_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

			material_cbuf_data.diffuse = material.diffuse;

			material_allocation = upload_buffer->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			material_allocation.Update(material_cbuf_data);
			cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

			D3D12_CPU_DESCRIPTOR_HANDLE diffuse_handle = texture_manager.CpuDescriptorHandle(material.diffuse_texture);
			u32 src_range_size = 1;

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
		
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		ResourceBarriers barrier{};

		blur_intermediate_texture.Transition(barrier, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		barrier.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(rs_map[RootSig::eBlur].Get());

		cmd_list->SetPipelineState(pso_map[PSO::eBlur_Horizontal].Get());

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

		cmd_list->Dispatch((u32)std::ceil(texture.Width() / 1024.0f), texture.Height(), 1);

		barrier.ReverseTransitions();
		blur_final_texture.Transition(barrier, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		barrier.Submit(cmd_list);

		//vertical pass

		cmd_list->SetPipelineState(pso_map[PSO::eBlur_Vertical].Get());

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
	void Renderer::CopyTexture(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture, BlendMode mode)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eCopy].Get());

		switch (mode)
		{
		case BlendMode::eNone:
			cmd_list->SetPipelineState(pso_map[PSO::eCopy].Get());
			break;
		case BlendMode::eAlphaBlend:
			cmd_list->SetPipelineState(pso_map[PSO::eCopy_AlphaBlend].Get());
			break;
		case BlendMode::eAdditiveBlend:
			cmd_list->SetPipelineState(pso_map[PSO::eCopy_AdditiveBlend].Get());
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
	void Renderer::AddTextures(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture1, Texture2D const& texture2, BlendMode mode)
	{
		auto device = gfx->Device();
		auto descriptor_allocator = gfx->DescriptorAllocator();

		cmd_list->SetGraphicsRootSignature(rs_map[RootSig::eAdd].Get());

		switch (mode)
		{
		case BlendMode::eNone:
			cmd_list->SetPipelineState(pso_map[PSO::eAdd].Get());
			break;
		case BlendMode::eAlphaBlend:
			cmd_list->SetPipelineState(pso_map[PSO::eAdd_AlphaBlend].Get());
			break;
		case BlendMode::eAdditiveBlend:
			cmd_list->SetPipelineState(pso_map[PSO::eAdd_AdditiveBlend].Get());
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









//transparent shadows
/*
for (auto e : entities)
		{

			if (reg->HasComponent<ShadowVisibleTag>(e) || !frustum_culler)
			{

				auto const& transformation = reg->GetComponent<TransformComponent>(e);
				auto const& mesh = reg->GetComponent<MeshComponent>(e);

				object_cbuffer_data.model = transformation.transform;
				object_cbuffer_data.inverse_transpose_model = DirectX::XMMatrixInverse(nullptr, object_cbuffer_data.model);
				Globals::UpdateCBuffer(context, object_cbuffer_data);


				bool has_diffuse = false;
				TEXTURE_HANDLE tex_handle = INVALID_TEXTURE_HANDLE;

				bool maybe_transparent = true;
				if (reg->HasComponent<ForwardTag>(e)) maybe_transparent = reg->GetComponent<ForwardTag>(e).transparent;

				if (maybe_transparent && reg->HasComponent<MaterialComponent>(e))
				{
					const auto& material = reg->GetComponent<MaterialComponent>(e);

					has_diffuse = material.diffuse_texture != INVALID_TEXTURE_HANDLE;
					if (has_diffuse)
						tex_handle = material.diffuse_texture;

				}
				else if (maybe_transparent && reg->HasComponent<PBRMaterialComponent>(e))
				{
					const auto& material = reg->GetComponent<PBRMaterialComponent>(e);

					has_diffuse = material.albedo_texture != INVALID_TEXTURE_HANDLE;
					if (has_diffuse)
						tex_handle = material.albedo_texture;
				}

				if (has_diffuse)
				{
					auto view = texture_manager.GetTextureView(tex_handle);

					context->PSSetShaderResources(TEXTURE_SLOT_DIFFUSE, 1, &view);

					transparent_shadow_program.Bind(context);
				}
				else shadow_program.Bind(context);

				mesh.Draw(context);
			}
		}
*/
