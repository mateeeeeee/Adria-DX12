#pragma once
#include <memory>
#include "Enums.h"
#include "../Core/Definitions.h"
#include <DirectXCollision.h>
#include "../Graphics/VertexTypes.h"
#include "../Graphics/Buffer.h"
#include "../Graphics/TextureManager.h"
#include "entt/entity/entity.hpp"

#define COMPONENT 

namespace adria
{
	struct COMPONENT Transform
	{
		DirectX::XMMATRIX starting_transform = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX current_transform = DirectX::XMMatrixIdentity();
	};

	struct COMPONENT Mesh
	{
		std::shared_ptr<Buffer>		vertex_buffer = nullptr;
		std::shared_ptr<Buffer>		index_buffer = nullptr;
		std::shared_ptr<Buffer>		instance_buffer = nullptr;
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

	struct COMPONENT Relationship
	{
		static constexpr size_t MAX_CHILDREN = 2048;
		entt::entity parent = entt::null;
		size_t children_count = 0;
		entt::entity children[MAX_CHILDREN] = { entt::null };
	};

	struct COMPONENT Material
	{
		TextureHandle albedo_texture			  = INVALID_TEXTURE_HANDLE;
		TextureHandle normal_texture			  = INVALID_TEXTURE_HANDLE;
		TextureHandle metallic_roughness_texture  = INVALID_TEXTURE_HANDLE;
		TextureHandle emissive_texture			  = INVALID_TEXTURE_HANDLE;

		float32 albedo_factor		= 1.0f;
		float32 metallic_factor		= 1.0f;
		float32 roughness_factor	= 1.0f;
		float32 emissive_factor		= 1.0f;

		DirectX::XMFLOAT3 diffuse = DirectX::XMFLOAT3(1, 1, 1);
		EPipelineState pso = EPipelineState::Unknown;
	};

	struct COMPONENT Light
	{
		DirectX::XMVECTOR position	= DirectX::XMVectorSet(0, 0, 0, 1);
		DirectX::XMVECTOR direction	= DirectX::XMVectorSet(0, -1, 0, 0);
		DirectX::XMVECTOR color		= DirectX::XMVectorSet(1, 1, 1, 1);
		float32 range = 100.0f;
		float32 energy = 1.0f;
		ELightType type = ELightType::Directional;
		float32 outer_cosine;
		float32 inner_cosine;
		bool casts_shadows = false;
		bool use_cascades = false;
		bool ray_traced_shadows = false;
		bool soft_rts = false;
		bool active = true;
		float32 volumetric_strength = 1.0f;
		bool volumetric = false;
		bool lens_flare = false;
		bool god_rays = false;
		float32 godrays_decay = 0.825f;
		float32 godrays_weight = 0.25f;
		float32 godrays_density = 0.975f;
		float32 godrays_exposure = 2.0f;
		bool	sscs = false;
		float32 sscs_thickness = 0.5f;
		float32 sscs_max_ray_distance = 0.05f;
		float32 sscs_max_depth_distance = 200.0f;
	};

	struct COMPONENT AABB
	{
		DirectX::BoundingBox bounding_box;
		bool camera_visible = true;
		bool light_visible = true;
	};

	struct COMPONENT Skybox
	{
		TextureHandle cubemap_texture;
		bool active;
		bool used_in_rt;
	};

	struct COMPONENT Emitter
	{
		TextureHandle		particle_texture = INVALID_TEXTURE_HANDLE;
		DirectX::XMFLOAT4	position = DirectX::XMFLOAT4(0, 0, 0, 0);
		DirectX::XMFLOAT4	velocity = DirectX::XMFLOAT4(0, 5, 0, 0);
		DirectX::XMFLOAT4	position_variance = DirectX::XMFLOAT4(0, 0, 0, 0);
		int32				number_to_emit = 0;
		float32				particle_lifespan = 5.0f;
		float32				start_size = 10.0f;
		float32				end_size = 1.0f;
		float32				mass = 1.0f;
		float32				velocity_variance = 1.0f;
		float32				particles_per_second = 100;
		float32				accumulation = 0.0f;
		float32				elapsed_time = 0.0f;
		bool				collisions_enabled = false;
		int32				collision_thickness = 40;
		bool				alpha_blended = true;
		bool				pause = false;
		bool				sort = false;
		mutable bool		reset_emitter = true;
	};

	struct COMPONENT Decal
	{
		TextureHandle albedo_decal_texture = INVALID_TEXTURE_HANDLE;
		TextureHandle normal_decal_texture = INVALID_TEXTURE_HANDLE;
		DirectX::XMMATRIX decal_model_matrix = DirectX::XMMatrixIdentity();
		EDecalType decal_type = EDecalType::Project_XY;
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

	struct COMPONENT RayTracing
	{
		friend class ModelImporter;
		friend class RayTracer;

	public:
		uint32 vertex_offset; //offset to global raytracing vertex buffer
		uint32 index_offset;  //offset to global raytracing index buffer

	private:
		inline static std::vector<CompleteVertex> rt_vertices = {};
		inline static std::vector<uint32> rt_indices = {};
	};
}