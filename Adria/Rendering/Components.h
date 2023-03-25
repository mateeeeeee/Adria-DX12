#pragma once
#include <memory>
#include <DirectXCollision.h>
#include "Enums.h"
#include "GeometryBufferCache.h"
#include "../Core/CoreTypes.h"
#include "../Graphics/GfxVertexTypes.h"
#include "../Graphics/GfxBuffer.h"
#include "TextureManager.h"
#include "entt/entity/entity.hpp"

#define COMPONENT

namespace adria
{
	struct COMPONENT Transform
	{
		DirectX::XMMATRIX current_transform = DirectX::XMMatrixIdentity();
	};
	struct COMPONENT Mesh
	{
		std::shared_ptr<GfxBuffer>		vertex_buffer = nullptr;
		std::shared_ptr<GfxBuffer>		index_buffer = nullptr;
		std::shared_ptr<GfxBuffer>		instance_buffer = nullptr;
		//only vb
		uint32 vertex_count = 0;
		uint32 start_vertex_location = 0; //Index of the first vertex

		//vb/ib
		uint32 indices_count = 0;
		uint32 start_index_location = 0; //The location of the first index read by the GPU from the index buffer
		int32 base_vertex_location = 0;  //A value added to each index before reading a vertex from the vertex buffer

		//instancing
		uint32 instance_count = 1;
		uint32 start_instance_location = 0; //A value added to each index before reading per-instance data from a vertex buffer

		D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		void Draw(ID3D12GraphicsCommandList* cmd_list) const
		{
			cmd_list->IASetPrimitiveTopology(topology);
			BindVertexBuffer(cmd_list, vertex_buffer.get());
			if (index_buffer)
			{
				BindIndexBuffer(cmd_list, index_buffer.get());
				cmd_list->DrawIndexedInstanced(indices_count, instance_count, start_index_location, base_vertex_location, start_instance_location);
			}
			else cmd_list->DrawInstanced(vertex_count, instance_count, start_vertex_location, start_instance_location);
		}
		void Draw(ID3D12GraphicsCommandList* cmd_list, D3D12_PRIMITIVE_TOPOLOGY override_topology) const
		{
			cmd_list->IASetPrimitiveTopology(override_topology);
			BindVertexBuffer(cmd_list, vertex_buffer.get());
			if (index_buffer)
			{
				BindIndexBuffer(cmd_list, index_buffer.get());
				cmd_list->DrawIndexedInstanced(indices_count, instance_count, start_index_location, base_vertex_location, start_instance_location);
			}
			else cmd_list->DrawInstanced(vertex_count, instance_count, start_vertex_location, start_instance_location);
		}
	};
	struct COMPONENT Material
	{
		TextureHandle albedo_texture			  = INVALID_TEXTURE_HANDLE;
		TextureHandle normal_texture			  = INVALID_TEXTURE_HANDLE;
		TextureHandle metallic_roughness_texture  = INVALID_TEXTURE_HANDLE;
		TextureHandle emissive_texture			  = INVALID_TEXTURE_HANDLE;

		float base_color[3]		= { 1.0f, 1.0f, 1.0f };
		float metallic_factor	= 1.0f;
		float roughness_factor	= 1.0f;
		float emissive_factor	= 1.0f;

		MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
		float alpha_cutoff	= 0.5f;
		bool  double_sided	= false;

		GfxPipelineStateID pso = GfxPipelineStateID::Unknown;
	};
	struct COMPONENT RayTracing
	{
		friend class EntityLoader;
		friend class Renderer;

	public:
		uint32 vertex_offset; //offset to global raytracing vertex buffer
		uint32 index_offset;  //offset to global raytracing index buffer

	private:
		inline static std::vector<CompleteVertex> rt_vertices = {};
		inline static std::vector<uint32> rt_indices = {};
	};

	struct COMPONENT AABB
	{
		DirectX::BoundingBox bounding_box;
		bool camera_visible = true;
		bool light_visible = true;
		bool draw_aabb = false;
		std::shared_ptr<GfxBuffer> aabb_vb = nullptr;

		void UpdateBuffer(GfxDevice* gfx)
		{
			DirectX::XMFLOAT3 corners[8];
			bounding_box.GetCorners(corners);
			SimpleVertex vertices[] =
			{
				SimpleVertex{corners[0]},
				SimpleVertex{corners[1]},
				SimpleVertex{corners[2]},
				SimpleVertex{corners[3]},
				SimpleVertex{corners[4]},
				SimpleVertex{corners[5]},
				SimpleVertex{corners[6]},
				SimpleVertex{corners[7]}
			};
			if (!aabb_vb)
			{
				GfxBufferDesc desc = VertexBufferDesc(ARRAYSIZE(vertices), sizeof(SimpleVertex));
				desc.resource_usage = GfxResourceUsage::Upload;
				aabb_vb = std::make_unique<GfxBuffer>(gfx, desc, vertices);
			}
			else
			{
				aabb_vb->Update(vertices, sizeof(vertices));
			}
		}
	};
	struct COMPONENT Relationship
	{
		size_t children_count = 0;
		entt::entity first = entt::null;
		entt::entity prev = entt::null;
		entt::entity next = entt::null;
		entt::entity parent = entt::null;
	};
	struct COMPONENT Light
	{
		DirectX::XMVECTOR position	= DirectX::XMVectorSet(0, 0, 0, 1);
		DirectX::XMVECTOR direction	= DirectX::XMVectorSet(0, -1, 0, 0);
		DirectX::XMVECTOR color		= DirectX::XMVectorSet(1, 1, 1, 1);
		float range = 100.0f;
		float energy = 1.0f;
		LightType type = LightType::Directional;
		float outer_cosine = 0.0f;
		float inner_cosine = 0.707f;
		bool active = true;

		bool casts_shadows = false;
		bool use_cascades = false;
		bool ray_traced_shadows = false;
		bool soft_rts = false;
		int32 shadow_texture_index = -1;
		int32 shadow_matrix_index = -1;
		int32 shadow_mask_index = -1;
		uint32 light_index = 0;

		float volumetric_strength = 0.3f;
		bool volumetric = false;
		bool lens_flare = false;
		bool god_rays = false;
		float godrays_decay = 0.825f;
		float godrays_weight = 0.25f;
		float godrays_density = 0.975f;
		float godrays_exposure = 2.0f;
		bool  sscs = false;
		float sscs_thickness = 0.5f;
		float sscs_max_ray_distance = 0.05f;
		float sscs_max_depth_distance = 200.0f;
	};
	struct COMPONENT Skybox
	{
		TextureHandle cubemap_texture;
		bool active;
		bool used_in_rt;
	};
	struct COMPONENT Decal
	{
		TextureHandle albedo_decal_texture = INVALID_TEXTURE_HANDLE;
		TextureHandle normal_decal_texture = INVALID_TEXTURE_HANDLE;
		DirectX::XMMATRIX decal_model_matrix = DirectX::XMMatrixIdentity();
		DecalType decal_type = DecalType::Project_XY;
		bool modify_gbuffer_normals = false;
	};
	struct COMPONENT Ocean {};
	struct COMPONENT Deferred {};
	struct COMPONENT Forward
	{
		bool transparent;
	};
	struct COMPONENT Tag
	{
		std::string name = "name tag";
	};

	struct COMPONENT SubMesh
	{
		GeometryBufferHandle geometry_buffer_handle = INVALID_GEOMETRY_BUFFER_HANDLE;

		uint32 positions_offset;
		uint32 uvs_offset;
		uint32 normals_offset;
		uint32 tangents_offset;
		uint32 bitangents_offset;
		uint32 indices_offset;
		uint32 meshlet_offset;
		uint32 meshlet_vertices_offset;
		uint32 meshlet_triangles_offset;
		uint32 meshlet_count;
	};



}