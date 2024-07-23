#pragma once
#include <memory>
#include <DirectXCollision.h>
#include "GeometryBufferCache.h"
#include "Graphics/GfxVertexFormat.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxStates.h"
#include "TextureManager.h"
#include "entt/entity/entity.hpp"

#define COMPONENT

namespace adria
{
	class GfxCommandList;

	enum class LightType : int32
	{
		Directional,
		Point,
		Spot
	};
	enum class DecalType : uint8
	{
		Project_XY,
		Project_YZ,
		Project_XZ
	};
	enum class MaterialAlphaMode : uint8
	{
		Opaque,
		Blend,
		Mask
	};

	struct COMPONENT Transform
	{
		Matrix current_transform = Matrix::Identity;
	};
	struct COMPONENT SubMesh
	{
		BoundingBox bounding_box;

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

		GfxPrimitiveTopology topology = GfxPrimitiveTopology::TriangleList;
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
	};
	struct COMPONENT Light
	{
		Vector4 position = Vector4(0, 0, 0, 1);
		Vector4 direction = Vector4(0, -1, 0, 0);
		Vector4 color = Vector4(1, 1, 1, 1);
		float range = 100.0f;
		float intensity = 1.0f;
		LightType type = LightType::Directional;
		float outer_cosine = 0.0f;
		float inner_cosine = 0.707f;
		bool active = true;

		bool casts_shadows = false;
		bool use_cascades = false;
		bool ray_traced_shadows = false;
		int32 shadow_texture_index = -1;
		int32 shadow_matrix_index = -1;
		int32 shadow_mask_index = -1;
		uint32 light_index = 0;

		float volumetric_strength = 0.004f;
		bool volumetric = false;
		bool lens_flare = false;
		bool god_rays = false;
		float godrays_decay = 0.9f;
		float godrays_weight = 0.65f;
		float godrays_density = 1.45f;
		float godrays_exposure = 3.25f;
		bool  sscs = false;
		float sscs_thickness = 0.5f;
		float sscs_max_ray_distance = 0.05f;
		float sscs_max_depth_distance = 200.0f;
	};
	struct COMPONENT Skybox
	{
		TextureHandle cubemap_texture;
		bool active;
	};
	struct COMPONENT Decal
	{
		TextureHandle albedo_decal_texture = INVALID_TEXTURE_HANDLE;
		TextureHandle normal_decal_texture = INVALID_TEXTURE_HANDLE;
		Matrix decal_model_matrix = Matrix::Identity;
		DecalType decal_type = DecalType::Project_XY;
		bool modify_gbuffer_normals = false;
	};
	struct COMPONENT Tag
	{
		std::string name = "name tag";
	};

	struct COMPONENT RayTracing {};
	struct COMPONENT Ocean {};
	struct COMPONENT Deferred {};

	struct SubMeshGPU
	{
		uint64 buffer_address;

		uint32 indices_offset;
		uint32 indices_count;
		uint32 vertices_count;

		uint32 positions_offset;
		uint32 uvs_offset;
		uint32 normals_offset;
		uint32 tangents_offset;

		uint32 meshlet_offset;
		uint32 meshlet_vertices_offset;
		uint32 meshlet_triangles_offset;
		uint32 meshlet_count;

		uint32 material_index;
		DirectX::BoundingBox bounding_box;
		GfxPrimitiveTopology topology;
	};
	struct SubMeshInstance
	{
		entt::entity parent;
		uint32 submesh_index;
		Matrix world_transform;
	};
	struct COMPONENT Mesh
	{
		ArcGeometryBufferHandle geometry_buffer_handle;
		std::vector<Material> materials;
		std::vector<SubMeshGPU> submeshes;
		std::vector<SubMeshInstance> instances;
	};

	struct COMPONENT Batch
	{
		uint32   instance_id;
		SubMeshGPU*  submesh;
		MaterialAlphaMode alpha_mode;
		Matrix world_transform;
		BoundingBox bounding_box;

		bool camera_visibility = true;
	};

	void Draw(SubMesh const& submesh, GfxCommandList* cmd_list, bool override_topology = false, GfxPrimitiveTopology new_topology = GfxPrimitiveTopology::Undefined);
}