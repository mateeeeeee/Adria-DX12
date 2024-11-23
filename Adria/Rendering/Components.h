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

	enum class LightType : Sint32
	{
		Directional,
		Point,
		Spot
	};
	enum class DecalType : Uint8
	{
		Project_XY,
		Project_YZ,
		Project_XZ
	};
	enum class MaterialAlphaMode : Uint8
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
		Uint32 vertex_count = 0;
		Uint32 start_vertex_location = 0; //Index of the first vertex

		//vb/ib
		Uint32 indices_count = 0;
		Uint32 start_index_location = 0; //The location of the first index read by the GPU from the index buffer
		Sint32 base_vertex_location = 0;  //A value added to each index before reading a vertex from the vertex buffer

		//instancing
		Uint32 instance_count = 1;
		Uint32 start_instance_location = 0; //A value added to each index before reading per-instance data from a vertex buffer

		GfxPrimitiveTopology topology = GfxPrimitiveTopology::TriangleList;
	};
	struct COMPONENT Material
	{
		TextureHandle albedo_texture			  = INVALID_TEXTURE_HANDLE;
		TextureHandle normal_texture			  = INVALID_TEXTURE_HANDLE;
		TextureHandle metallic_roughness_texture  = INVALID_TEXTURE_HANDLE;
		TextureHandle emissive_texture			  = INVALID_TEXTURE_HANDLE;

		Float base_color[3]		= { 1.0f, 1.0f, 1.0f };
		Float metallic_factor	= 1.0f;
		Float roughness_factor	= 1.0f;
		Float emissive_factor	= 1.0f;

		MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
		Float alpha_cutoff	= 0.5f;
		Bool  double_sided	= false;
	};
	struct COMPONENT Light
	{
		Vector4 position = Vector4(0, 0, 0, 1);
		Vector4 direction = Vector4(0, -1, 0, 0);
		Vector4 color = Vector4(1, 1, 1, 1);
		Float range = 100.0f;
		Float intensity = 1.0f;
		LightType type = LightType::Directional;
		Float outer_cosine = 0.0f;
		Float inner_cosine = 0.707f;
		Bool active = true;

		Bool casts_shadows = false;
		Bool use_cascades = false;
		Bool ray_traced_shadows = false;
		Sint32 shadow_texture_index = -1;
		Sint32 shadow_matrix_index = -1;
		Sint32 shadow_mask_index = -1;
		Uint32 light_index = 0;

		Float volumetric_strength = 0.004f;
		Bool volumetric = false;
		Bool lens_flare = false;
		Bool god_rays = false;
		Float godrays_decay = 0.9f;
		Float godrays_weight = 0.65f;
		Float godrays_density = 1.45f;
		Float godrays_exposure = 3.25f;
	};
	struct COMPONENT Skybox
	{
		TextureHandle cubemap_texture;
		Bool active;
	};
	struct COMPONENT Decal
	{
		TextureHandle albedo_decal_texture = INVALID_TEXTURE_HANDLE;
		TextureHandle normal_decal_texture = INVALID_TEXTURE_HANDLE;
		Matrix decal_model_matrix = Matrix::Identity;
		DecalType decal_type = DecalType::Project_XY;
		Bool modify_gbuffer_normals = false;
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
		Uint64 buffer_address;

		Uint32 indices_offset;
		Uint32 indices_count;
		Uint32 vertices_count;

		Uint32 positions_offset;
		Uint32 uvs_offset;
		Uint32 normals_offset;
		Uint32 tangents_offset;

		Uint32 meshlet_offset;
		Uint32 meshlet_vertices_offset;
		Uint32 meshlet_triangles_offset;
		Uint32 meshlet_count;

		Uint32 material_index;
		DirectX::BoundingBox bounding_box;
		GfxPrimitiveTopology topology;
	};
	struct SubMeshInstance
	{
		entt::entity parent;
		Uint32 submesh_index;
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
		Uint32   instance_id;
		SubMeshGPU*  submesh;
		MaterialAlphaMode alpha_mode;
		Matrix world_transform;
		BoundingBox bounding_box;

		Bool camera_visibility = true;

	};

	void Draw(SubMesh const& submesh, GfxCommandList* cmd_list, Bool override_topology = false, GfxPrimitiveTopology new_topology = GfxPrimitiveTopology::Undefined);
}