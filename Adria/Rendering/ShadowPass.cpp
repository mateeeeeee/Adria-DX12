#include "ShadowPass.h"
#include "Camera.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/Texture.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{
	//static helpers
	namespace
	{
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
			float l = sphere_centerLS.x - scene_bounding_sphere.Radius;
			float b = sphere_centerLS.y - scene_bounding_sphere.Radius;
			float n = sphere_centerLS.z - scene_bounding_sphere.Radius;
			float r = sphere_centerLS.x + scene_bounding_sphere.Radius;
			float t = sphere_centerLS.y + scene_bounding_sphere.Radius;
			float f = sphere_centerLS.z + scene_bounding_sphere.Radius;

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
			shadowOrigin *= (ShadowPass::SHADOW_MAP_SIZE / 2.0f);

			XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);
			XMVECTOR roundOffset = roundedOrigin - shadowOrigin;

			roundOffset *= (2.0f / ShadowPass::SHADOW_MAP_SIZE);
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

		std::array<XMMATRIX, ShadowPass::SHADOW_CASCADE_COUNT> RecalculateProjectionMatrices(Camera const& camera, float32 split_lambda, std::array<float32, ShadowPass::SHADOW_CASCADE_COUNT>& split_distances)
		{
			float32 camera_near = camera.Near();
			float32 camera_far = camera.Far();
			float32 fov = camera.Fov();
			float32 ar = camera.AspectRatio();

			float32 f = 1.0f / ShadowPass::SHADOW_CASCADE_COUNT;

			for (uint32 i = 0; i < split_distances.size(); i++)
			{
				float32 fi = (i + 1) * f;
				float32 l = camera_near * pow(camera_far / camera_near, fi);
				float32 u = camera_near + (camera_far - camera_near) * fi;
				split_distances[i] = l * split_lambda + u * (1.0f - split_lambda);
			}

			std::array<XMMATRIX, ShadowPass::SHADOW_CASCADE_COUNT> projectionMatrices{};
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

			std::array<XMMATRIX, ShadowPass::SHADOW_CASCADE_COUNT> lightViewProjectionMatrices{};

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

			shadowOrigin *= (ShadowPass::SHADOW_CASCADE_MAP_SIZE / 2.0f);

			XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);
			XMVECTOR roundOffset = roundedOrigin - shadowOrigin;

			roundOffset *= (2.0f / ShadowPass::SHADOW_CASCADE_MAP_SIZE);

			roundOffset *= XMVectorSet(1.0, 1.0, 0.0, 0.0);

			P.r[3] += roundOffset;
			BoundingBox::CreateFromPoints(cull_box, XMVectorSet(l, b, n, 1.0f), XMVectorSet(r, t, f, 1.0f));
			cull_box.Transform(cull_box, XMMatrixInverse(nullptr, V));

			return { V,P };
		}
	}

	ShadowPass::ShadowPass(entt::registry& reg, TextureManager& texture_manager)
		: reg(reg), texture_manager(texture_manager), camera(nullptr)
	{}

	void ShadowPass::SetCamera(Camera const* _camera)
	{
		camera = _camera;
	}

	void ShadowPass::AddPass(RenderGraph& rg, Light const& light, size_t light_id)
	{
		ADRIA_ASSERT(light.casts_shadows);
		switch (light.type)
		{
		case ELightType::Directional:
			if (light.use_cascades) return ShadowMapPass_DirectionalCascades(rg, light, light_id);
			else return ShadowMapPass_Directional(rg, light, light_id);
		case ELightType::Spot:
			return ShadowMapPass_Spot(rg, light, light_id);
		case ELightType::Point:
			return ShadowMapPass_Point(rg, light, light_id);
		default:
			ADRIA_ASSERT(false);
		}
	}

	void ShadowPass::ShadowMapPass_Directional(RenderGraph& rg, Light const& light, size_t light_id, std::optional<DirectX::BoundingSphere> const& scene_bounding_sphere)
	{
		ADRIA_ASSERT(light.type == ELightType::Directional && light.use_cascades == false);
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();

		struct ShadowPassData
		{
			RGAllocationId alloc_id;
		};

		rg.AddPass<ShadowPassData>("Shadow Map Directional Pass",
			[=](ShadowPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc depth_desc{};
				depth_desc.width = SHADOW_MAP_SIZE;
				depth_desc.height = SHADOW_MAP_SIZE;
				depth_desc.format = EFormat::R32_TYPELESS;
				depth_desc.clear_value = ClearValue(1.0f, 0);

				builder.DeclareTexture(RG_RES_NAME_IDX(ShadowMap, light_id), depth_desc);
				builder.DeclareAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id), AllocDesc{ GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT });
				
				builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light_id), ERGLoadStoreAccessOp::Clear_Preserve);
				data.alloc_id = builder.UseAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id));
				builder.SetViewport(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
			},
			[=](ShadowPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto upload_buffer = gfx->GetDynamicAllocator();

				BoundingBox light_box;
				auto const& [V, P] = scene_bounding_sphere ? LightViewProjection_Directional(light, *scene_bounding_sphere, light_box)
					: LightViewProjection_Directional(light, *camera, light_box);
				ShadowCBuffer shadow_cbuf_data{};
				shadow_cbuf_data.lightview = V;
				shadow_cbuf_data.lightviewprojection = V * P;
				shadow_cbuf_data.shadow_map_size = SHADOW_MAP_SIZE;
				shadow_cbuf_data.softness = 1.0f;
				shadow_cbuf_data.shadow_matrices[0] = XMMatrixInverse(nullptr, camera->View()) * shadow_cbuf_data.lightviewprojection;

				DynamicAllocation& shadow_allocation = context.GetAllocation(data.alloc_id);
				shadow_allocation.Update(shadow_cbuf_data);

				LightFrustumCulling(ELightType::Directional, light_box, std::nullopt);
				ShadowMapPass_Common(gfx, cmd_list, shadow_allocation.gpu_address, false);
			});
	}

	void ShadowPass::ShadowMapPass_DirectionalCascades(RenderGraph& rg, Light const& light, size_t light_id)
	{
		ADRIA_ASSERT(light.type == ELightType::Directional && light.use_cascades == true);
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct ShadowPassData
		{
			RGAllocationId alloc_id;
		};

		std::array<float32, SHADOW_CASCADE_COUNT> split_distances;
		std::array<XMMATRIX, SHADOW_CASCADE_COUNT> proj_matrices = RecalculateProjectionMatrices(*camera, 0.5f, split_distances);
		std::array<XMMATRIX, SHADOW_CASCADE_COUNT> light_view_projections{};
		std::array<XMMATRIX, SHADOW_CASCADE_COUNT> light_views{};
		std::array<DirectX::BoundingBox, SHADOW_CASCADE_COUNT> light_bounding_boxes{};
		for (uint32 i = 0; i < SHADOW_CASCADE_COUNT; ++i)
		{
			auto const& [V, P] = LightViewProjection_Cascades(light, *camera, proj_matrices[i], light_bounding_boxes[i]);
			light_views[i] = V;
			light_view_projections[i] = V * P;
		}

		for (size_t i = 0; i < SHADOW_CASCADE_COUNT; ++i)
		{
			std::string name = "Shadow Map Cascade Pass " + std::to_string(i);
			rg.AddPass<ShadowPassData>(name.c_str(),
				[=](ShadowPassData& data, RenderGraphBuilder& builder)
				{
					if (i == 0)
					{
						RGTextureDesc depth_cascade_maps_desc{};
						depth_cascade_maps_desc.width = SHADOW_CASCADE_MAP_SIZE;
						depth_cascade_maps_desc.height = SHADOW_CASCADE_MAP_SIZE;
						depth_cascade_maps_desc.misc_flags = ETextureMiscFlag::None;
						depth_cascade_maps_desc.array_size = SHADOW_CASCADE_COUNT;
						depth_cascade_maps_desc.format = EFormat::R32_TYPELESS;
						depth_cascade_maps_desc.clear_value = ClearValue(1.0f, 0);
						builder.DeclareTexture(RG_RES_NAME_IDX(ShadowMap, light_id), depth_cascade_maps_desc);
						builder.DeclareAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id), AllocDesc{ GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT });
					}

					builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light_id), ERGLoadStoreAccessOp::Clear_Preserve, 0, -1, i, 1);
					data.alloc_id = builder.UseAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id));
					builder.SetViewport(SHADOW_CASCADE_MAP_SIZE, SHADOW_CASCADE_MAP_SIZE);
				},
				[=](ShadowPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					auto dynamic_allocator = gfx->GetDynamicAllocator();
					ShadowCBuffer shadow_cbuf_data{};
					shadow_cbuf_data.lightview = light_views[i];
					shadow_cbuf_data.lightviewprojection = light_view_projections[i];
					shadow_cbuf_data.softness = 1.0f;

					DynamicAllocation shadow_allocation = dynamic_allocator->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					shadow_allocation.Update(shadow_cbuf_data);

					LightFrustumCulling(ELightType::Directional, light_bounding_boxes[i], std::nullopt);
					ShadowMapPass_Common(gfx, cmd_list, shadow_allocation.gpu_address, false);

					if (i == 0)
					{
						ShadowCBuffer shadow_cbuf_data{};
						shadow_cbuf_data.shadow_map_size = SHADOW_CASCADE_MAP_SIZE;
						shadow_cbuf_data.shadow_matrices[0] = DirectX::XMMatrixInverse(nullptr, camera->View()) * light_view_projections[0];
						shadow_cbuf_data.shadow_matrices[1] = DirectX::XMMatrixInverse(nullptr, camera->View()) * light_view_projections[1];
						shadow_cbuf_data.shadow_matrices[2] = DirectX::XMMatrixInverse(nullptr, camera->View()) * light_view_projections[2];
						shadow_cbuf_data.shadow_matrices[3] = DirectX::XMMatrixInverse(nullptr, camera->View()) * light_view_projections[3];
						shadow_cbuf_data.split0 = split_distances[0];
						shadow_cbuf_data.split1 = split_distances[1];
						shadow_cbuf_data.split2 = split_distances[2];
						shadow_cbuf_data.split3 = split_distances[3];
						shadow_cbuf_data.softness = 1.0f;
						shadow_cbuf_data.visualize = static_cast<int>(false);

						shadow_allocation = context.GetAllocation(data.alloc_id);
						shadow_allocation.Update(shadow_cbuf_data);
					}

				});
		}
	}

	void ShadowPass::ShadowMapPass_Point(RenderGraph& rg, Light const& light, size_t light_id)
	{
		ADRIA_ASSERT(light.type == ELightType::Point);
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct ShadowPassData
		{
			RGAllocationId alloc_id;
		};

		for (size_t i = 0; i < 6; ++i)
		{
			std::string name = "Shadow Map Point Pass " + std::to_string(i);
			rg.AddPass<ShadowPassData>(name.c_str(),
				[=](ShadowPassData& data, RenderGraphBuilder& builder)
				{
					if (i == 0)
					{
						RGTextureDesc cubemap_desc{};
						cubemap_desc.width = SHADOW_CUBE_SIZE;
						cubemap_desc.height = SHADOW_CUBE_SIZE;
						cubemap_desc.misc_flags = ETextureMiscFlag::TextureCube;
						cubemap_desc.array_size = 6;
						cubemap_desc.format = EFormat::R32_TYPELESS;
						cubemap_desc.clear_value = ClearValue(1.0f, 0);
						builder.DeclareTexture(RG_RES_NAME_IDX(ShadowMap, light_id), cubemap_desc);
						builder.DeclareAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id), AllocDesc{ GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT });
					}

					builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light_id), ERGLoadStoreAccessOp::Clear_Preserve, 0, -1, i, 1);
					data.alloc_id = builder.UseAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id));
					builder.SetViewport(SHADOW_CUBE_SIZE, SHADOW_CUBE_SIZE);
				},
				[=](ShadowPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					auto dynamic_allocator = gfx->GetDynamicAllocator();

					DirectX::BoundingFrustum light_bounding_frustum;
					auto const& [V, P] = LightViewProjection_Point(light, i, light_bounding_frustum);
					ShadowCBuffer shadow_cbuf_data{};
					shadow_cbuf_data.lightviewprojection = V * P;
					shadow_cbuf_data.lightview = V;

					DynamicAllocation shadow_allocation = dynamic_allocator->Allocate(GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					shadow_allocation.Update(shadow_cbuf_data);

					LightFrustumCulling(ELightType::Point, std::nullopt, light_bounding_frustum);
					ShadowMapPass_Common(gfx, cmd_list, shadow_allocation.gpu_address, false);
				});
		}
	}

	void ShadowPass::ShadowMapPass_Spot(RenderGraph& rg, Light const& light, size_t light_id)
	{
		ADRIA_ASSERT(light.type == ELightType::Spot);
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct ShadowPassData
		{
			RGAllocationId alloc_id;
		};
		rg.AddPass<ShadowPassData>("Shadow Map Spot Pass",
			[=](ShadowPassData& data, RenderGraphBuilder& builder)
			{
				D3D12_CLEAR_VALUE clear_value{};
				clear_value.Format = DXGI_FORMAT_D32_FLOAT;
				clear_value.DepthStencil.Depth = 1.0f;
				clear_value.DepthStencil.Stencil = 0;

				RGTextureDesc depth_desc{};
				depth_desc.width = SHADOW_MAP_SIZE;
				depth_desc.height = SHADOW_MAP_SIZE;
				depth_desc.format = EFormat::R32_TYPELESS;
				depth_desc.clear_value = ClearValue(1.0f, 0);

				builder.DeclareTexture(RG_RES_NAME_IDX(ShadowMap, light_id), depth_desc);
				builder.DeclareAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id), AllocDesc{ GetCBufferSize<ShadowCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT });
				builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light_id), ERGLoadStoreAccessOp::Clear_Preserve);
				data.alloc_id = builder.UseAllocation(RG_RES_NAME_IDX(ShadowAllocation, light_id));
				builder.SetViewport(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
			},
			[=](ShadowPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto upload_buffer = gfx->GetDynamicAllocator();

				DirectX::BoundingFrustum light_bounding_frustum;
				auto const& [V, P] = LightViewProjection_Spot(light, light_bounding_frustum);
				ShadowCBuffer shadow_cbuf_data{};
				shadow_cbuf_data.lightview = V;
				shadow_cbuf_data.lightviewprojection = V * P;
				shadow_cbuf_data.shadow_map_size = SHADOW_MAP_SIZE;
				shadow_cbuf_data.softness = 1.0f;
				shadow_cbuf_data.shadow_matrices[0] = XMMatrixInverse(nullptr, camera->View()) * shadow_cbuf_data.lightviewprojection;

				DynamicAllocation& shadow_allocation = context.GetAllocation(data.alloc_id);
				shadow_allocation.Update(shadow_cbuf_data);

				LightFrustumCulling(ELightType::Spot, std::nullopt, light_bounding_frustum);
				ShadowMapPass_Common(gfx, cmd_list, shadow_allocation.gpu_address, false);
			});
	}

	void ShadowPass::ShadowMapPass_Common(GraphicsDevice* gfx, ID3D12GraphicsCommandList4* cmd_list,
		D3D12_GPU_VIRTUAL_ADDRESS allocation_address, bool transparent)
	{
		ID3D12Device* device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
		auto upload_buffer = gfx->GetDynamicAllocator();

		auto shadow_view = reg.view<Mesh, Transform, AABB>();
		if (!transparent)
		{
			cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::DepthMap));
			cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::DepthMap));
			cmd_list->SetGraphicsRootConstantBufferView(1, allocation_address);

			for (auto e : shadow_view)
			{
				auto const& aabb = shadow_view.get<AABB>(e);
				if (aabb.light_visible)
				{
					auto const& transform = shadow_view.get<Transform>(e);
					auto const& mesh = shadow_view.get<Mesh>(e);

					DirectX::XMMATRIX parent_transform = DirectX::XMMatrixIdentity();
					if (Relationship* relationship = reg.try_get<Relationship>(e))
					{
						if (auto* root_transform = reg.try_get<Transform>(relationship->parent)) parent_transform = root_transform->current_transform;
					}

					ObjectCBuffer object_cbuf_data{};
					object_cbuf_data.model = transform.current_transform * parent_transform;
					object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

					DynamicAllocation object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					object_allocation.Update(object_cbuf_data);
					cmd_list->SetGraphicsRootConstantBufferView(0, object_allocation.gpu_address);

					mesh.Draw(cmd_list);
				}
			}
		}
		else
		{
			std::vector<entt::entity> potentially_transparent, not_transparent;
			for (auto e : shadow_view)
			{
				auto const& aabb = shadow_view.get<AABB>(e);
				if (aabb.light_visible)
				{
					if (auto* p_material = reg.try_get<Material>(e))
					{
						if (p_material->albedo_texture != INVALID_TEXTURE_HANDLE)
							potentially_transparent.push_back(e);
						else not_transparent.push_back(e);
					}
					else not_transparent.push_back(e);
				}
			}

			cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::DepthMap));
			cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::DepthMap));
			cmd_list->SetGraphicsRootConstantBufferView(1, allocation_address);
			for (auto e : not_transparent)
			{
				auto& transform = shadow_view.get<Transform>(e);
				auto& mesh = shadow_view.get<Mesh>(e);

				DirectX::XMMATRIX parent_transform = DirectX::XMMatrixIdentity();
				if (Relationship* relationship = reg.try_get<Relationship>(e))
				{
					if (auto* root_transform = reg.try_get<Transform>(relationship->parent)) parent_transform = root_transform->current_transform;
				}

				ObjectCBuffer object_cbuf_data{};
				object_cbuf_data.model = transform.current_transform * parent_transform;
				object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

				DynamicAllocation object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				object_allocation.Update(object_cbuf_data);
				cmd_list->SetGraphicsRootConstantBufferView(0, object_allocation.gpu_address);
				mesh.Draw(cmd_list);
			}

			cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::DepthMap_Transparent));
			cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::DepthMap_Transparent));
			cmd_list->SetGraphicsRootConstantBufferView(1, allocation_address);

			for (auto e : potentially_transparent)
			{
				auto& transform = shadow_view.get<Transform>(e);
				auto& mesh = shadow_view.get<Mesh>(e);
				auto* material = reg.try_get<Material>(e);
				ADRIA_ASSERT(material != nullptr);
				ADRIA_ASSERT(material->albedo_texture != INVALID_TEXTURE_HANDLE);

				DirectX::XMMATRIX parent_transform = DirectX::XMMatrixIdentity();
				if (Relationship* relationship = reg.try_get<Relationship>(e))
				{
					if (auto* root_transform = reg.try_get<Transform>(relationship->parent)) parent_transform = root_transform->current_transform;
				}
				ObjectCBuffer object_cbuf_data{};
				object_cbuf_data.model = transform.current_transform * parent_transform;
				object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);

				DynamicAllocation object_allocation = upload_buffer->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
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

	void ShadowPass::LightFrustumCulling(ELightType type, std::optional<DirectX::BoundingBox> const& light_bounding_box, std::optional<DirectX::BoundingFrustum> const& light_bounding_frustum)
	{
		auto aabb_view = reg.view<AABB>();

		for (auto e : aabb_view)
		{
			auto& aabb = aabb_view.get<AABB>(e);
			switch (type)
			{
			case ELightType::Directional:
				ADRIA_ASSERT(light_bounding_box.has_value());
				aabb.light_visible = light_bounding_box->Intersects(aabb.bounding_box);
				break;
			case ELightType::Spot:
			case ELightType::Point:
				ADRIA_ASSERT(light_bounding_frustum.has_value());
				aabb.light_visible = light_bounding_frustum->Intersects(aabb.bounding_box);
				break;
			default:
				ADRIA_ASSERT(false);
			}
		}
	}

}
