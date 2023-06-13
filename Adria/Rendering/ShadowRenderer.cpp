#include "ShadowRenderer.h"
#include "Components.h"
#include "Camera.h"
#include "PSOCache.h"
#include "BlackboardData.h"
#include "ShaderStructs.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "RenderGraph/RenderGraph.h"

using namespace DirectX;

namespace adria
{
	namespace 
	{

		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Directional(Light const& light, Camera const& camera, uint32 shadow_size)
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
			shadow_origin *= (shadow_size / 2.0f);

			XMVECTOR rounded_origin = XMVectorRound(shadow_origin);
			XMVECTOR rounded_offset = rounded_origin - shadow_origin;
			rounded_offset *= (2.0f / shadow_size);
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
		std::pair<XMMATRIX, XMMATRIX> LightViewProjection_Cascades(Light const& light, Camera const& camera, XMMATRIX projection_matrix, uint32 shadow_cascade_size)
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
			shadow_origin *= (shadow_cascade_size / 2.0f);

			XMVECTOR rounded_origin = XMVectorRound(shadow_origin);
			XMVECTOR round_offset = rounded_origin - shadow_origin;

			round_offset *= (2.0f / shadow_cascade_size);
			round_offset *= XMVectorSet(1.0, 1.0, 0.0, 0.0);
			P.r[3] += round_offset;
			return { V,P };
		}
	}

	ShadowRenderer::ShadowRenderer(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height) : reg(reg), gfx(gfx), width(width), height(height),
		ray_traced_shadows_pass(gfx, width, height)
	{

	}
	ShadowRenderer::~ShadowRenderer()
	{

	}

	void ShadowRenderer::FillFrameCBuffer(FrameCBuffer& frame_cbuffer)
	{
		frame_cbuffer.lights_matrices_idx = light_matrices_gpu_index;
		frame_cbuffer.cascade_splits = XMVectorSet(split_distances[0], split_distances[1], split_distances[2], split_distances[3]);
	}
	void ShadowRenderer::SetupShadows(Camera const* camera)
	{
		static constexpr uint32 backbuffer_count = GFX_BACKBUFFER_COUNT;
		uint32 backbuffer_index = gfx->GetBackbufferIndex();

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
			GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU();
			gfx->CopyDescriptors(1, dst_descriptor, srv);
			light.shadow_mask_index = (int32)dst_descriptor.GetIndex();
		};
		auto AddShadowMap  = [&](size_t light_id, uint32 shadow_map_size)
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
			switch (light.type)
			{
			case LightType::Directional:
			{
				if (light.use_cascades && light_shadow_maps[light_id].size() != SHADOW_CASCADE_COUNT)
				{
					light_shadow_maps[light_id].clear();
					for (uint32 i = 0; i < SHADOW_CASCADE_COUNT; ++i) AddShadowMap(light_id, SHADOW_CASCADE_MAP_SIZE);
				}
				else if (!light.use_cascades && light_shadow_maps[light_id].size() != 1)
				{
					light_shadow_maps[light_id].clear();
					AddShadowMap(light_id, SHADOW_MAP_SIZE);
				}
			}
			break;
			case LightType::Point:
			{
				if (light_shadow_maps[light_id].size() != 6)
				{
					light_shadow_maps[light_id].clear();
					for (uint32 i = 0; i < 6; ++i) AddShadowMap(light_id, SHADOW_CUBE_SIZE);
				}
			}
			break;
			case LightType::Spot:
			{
				if (light_shadow_maps[light_id].size() != 1)
				{
					light_shadow_maps[light_id].clear();
					AddShadowMap(light_id, SHADOW_MAP_SIZE);
				}
			}
			break;
			}

			for (size_t j = 0; j < light_shadow_maps[light_id].size(); ++j)
			{
				GfxDescriptor srv = light_shadow_map_srvs[light_id][j];
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU();
				gfx->CopyDescriptors(1, dst_descriptor, srv);
				if (j == 0) light.shadow_texture_index = (int32)dst_descriptor.GetIndex();
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
				if (light.type == LightType::Directional && light.use_cascades) current_light_matrices_count += SHADOW_CASCADE_COUNT;
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

		std::vector<XMMATRIX> _light_matrices;
		_light_matrices.reserve(light_matrices_count);
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			light.shadow_mask_index = -1;
			light.shadow_texture_index = -1;
			if (light.casts_shadows)
			{
				if (light.ray_traced_shadows) continue;
				light.shadow_matrix_index = (uint32)_light_matrices.size();
				if (light.type == LightType::Directional)
				{
					if (light.use_cascades)
					{
						std::array<XMMATRIX, SHADOW_CASCADE_COUNT> proj_matrices = RecalculateProjectionMatrices(*camera, cascades_split_lambda, split_distances);
						for (uint32 i = 0; i < SHADOW_CASCADE_COUNT; ++i)
						{
							AddShadowMaps(light, entt::to_integral(e));
							auto const& [V, P] = LightViewProjection_Cascades(light, *camera, proj_matrices[i], SHADOW_CASCADE_MAP_SIZE);
							_light_matrices.push_back(XMMatrixTranspose(V * P));
						}
					}
					else
					{
						AddShadowMaps(light, entt::to_integral(e));
						auto const& [V, P] = LightViewProjection_Directional(light, *camera, SHADOW_MAP_SIZE);
						_light_matrices.push_back(XMMatrixTranspose(V * P));
					}

				}
				else if (light.type == LightType::Point)
				{
					for (uint32 i = 0; i < 6; ++i)
					{
						AddShadowMaps(light, entt::to_integral(e));
						auto const& [V, P] = LightViewProjection_Point(light, i);
						_light_matrices.push_back(XMMatrixTranspose(V * P));
					}
				}
				else if (light.type == LightType::Spot)
				{
					AddShadowMaps(light, entt::to_integral(e));
					auto const& [V, P] = LightViewProjection_Spot(light);
					_light_matrices.push_back(XMMatrixTranspose(V * P));
				}
			}
			else if (light.ray_traced_shadows)
			{
				AddShadowMask(light, entt::to_integral(e));
			}
		}
		if (light_matrices_buffer)
		{
			light_matrices_buffer->Update(_light_matrices.data(), light_matrices_count * sizeof(XMMATRIX), light_matrices_count * sizeof(XMMATRIX) * backbuffer_index);
			GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU();
			gfx->CopyDescriptors(1, dst_descriptor, light_matrices_buffer_srvs[backbuffer_index]);
			light_matrices_gpu_index = (int32)dst_descriptor.GetIndex();
		}
		light_matrices = std::move(_light_matrices);
	}

	void ShadowRenderer::AddShadowMapPasses(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		auto light_view = reg.view<Light>();
		for (auto e : light_view)
		{
			auto& light = light_view.get<Light>(e);
			if (!light.casts_shadows) continue;
			int32 light_index = light.light_index;
			int32 light_matrix_index = light.shadow_matrix_index;
			size_t light_id = entt::to_integral(e);

			if (light.type == LightType::Directional)
			{
				if (light.use_cascades)
				{
					for (uint32 i = 0; i < SHADOW_CASCADE_COUNT; ++i)
					{
						rg.ImportTexture(RG_RES_NAME_IDX(ShadowMap, light_matrix_index + i), light_shadow_maps[light_id][i].get());
						std::string name = "Cascade Shadow Pass" + std::to_string(i);
						rg.AddPass<void>(name.c_str(),
							[=](RenderGraphBuilder& builder)
							{
								builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light_matrix_index + i), RGLoadStoreAccessOp::Clear_Preserve);
								builder.SetViewport(SHADOW_CASCADE_MAP_SIZE, SHADOW_CASCADE_MAP_SIZE);
							},
							[=](RenderGraphContext& context, GfxCommandList* cmd_list)
							{
								cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
								ShadowMapPass_Common(gfx, cmd_list, light_index, light_matrix_index, i);
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
							builder.SetViewport(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
						},
						[=](RenderGraphContext& context, GfxCommandList* cmd_list)
						{
							cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
							ShadowMapPass_Common(gfx, cmd_list, light_index, light_matrix_index, 0);
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
							builder.SetViewport(SHADOW_CUBE_SIZE, SHADOW_CUBE_SIZE);
						},
						[=](RenderGraphContext& context, GfxCommandList* cmd_list)
						{
							cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
							ShadowMapPass_Common(gfx, cmd_list, light_index, light_matrix_index, i);
						}, RGPassType::Graphics);
				}
			}
			else if (light.type == LightType::Spot)
			{
				rg.ImportTexture(RG_RES_NAME_IDX(ShadowMap, light_matrix_index), light_shadow_maps[light_id][0].get());
				std::string name = "Spot Shadow Pass";
				rg.AddPass<void>(name.c_str(),
					[=](RenderGraphBuilder& builder)
					{
						builder.WriteDepthStencil(RG_RES_NAME_IDX(ShadowMap, light_matrix_index), RGLoadStoreAccessOp::Clear_Preserve);
						builder.SetViewport(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
					},
					[=](RenderGraphContext& context, GfxCommandList* cmd_list)
					{
						cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
						ShadowMapPass_Common(gfx, cmd_list, light_index, light_matrix_index, 0);
					}, RGPassType::Graphics);
			}
		}
	}
	void ShadowRenderer::AddRayTracingShadowPasses(RenderGraph& rg)
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

	void ShadowRenderer::ShadowMapPass_Common(GfxDevice* gfx, GfxCommandList* cmd_list, size_t light_index, size_t matrix_index, size_t matrix_offset)
	{
		struct ShadowConstants
		{
			uint32  light_index;
			uint32  matrix_offset;
		} constants =
		{
			.light_index = (uint32)light_index,
			.matrix_offset = (uint32)matrix_offset
		};
		auto view = reg.view<Batch>();
		std::vector<Batch*> masked_batches, opaque_batches;
		for (auto batch_entity : reg.view<Batch>())
		{
			Batch& batch = reg.get<Batch>(batch_entity);
			if (batch.alpha_mode == MaterialAlphaMode::Opaque) opaque_batches.push_back(&batch);
			else masked_batches.push_back(&batch);
		}

		auto DrawBatch = [&](GfxCommandList* cmd_list, bool masked_batch) 
		{
			std::vector<Batch*>& batches = masked_batch ? masked_batches : opaque_batches;
			GfxPipelineStateID pso = masked_batch ? GfxPipelineStateID::Shadow_Transparent : GfxPipelineStateID::Shadow;
			cmd_list->SetRootConstants(1, constants);
			cmd_list->SetPipelineState(PSOCache::Get(pso));
			for (Batch* batch : batches)
			{
				struct ModelConstants
				{
					uint32 instance_id;
				} model_constants{ .instance_id = batch->instance_id };
				cmd_list->SetRootCBV(2, model_constants);
				GfxIndexBufferView ibv(batch->submesh->buffer_address + batch->submesh->indices_offset, batch->submesh->indices_count);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
				cmd_list->SetIndexBuffer(&ibv);
				cmd_list->DrawIndexed(batch->submesh->indices_count);
			}
		};

		DrawBatch(cmd_list, false);
		DrawBatch(cmd_list, true);
	}
	std::array<XMMATRIX, ShadowRenderer::SHADOW_CASCADE_COUNT> ShadowRenderer::RecalculateProjectionMatrices(Camera const& camera, float split_lambda, std::array<float, SHADOW_CASCADE_COUNT>& split_distances)
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

}

