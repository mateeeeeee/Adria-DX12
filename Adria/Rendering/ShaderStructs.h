#pragma once

#ifndef DECLSPEC_ALIGN
#define DECLSPEC_ALIGN(x)   __declspec(align(x))
#endif

#define PAD uint32 ADRIA_CONCAT(pad, __COUNTER__)

namespace adria
{
	using bool32 = int32;

	DECLSPEC_ALIGN(16) struct FrameCBuffer
	{
		Matrix view;
		Matrix projection;
		Matrix view_projection;
		Matrix inverse_view;
		Matrix inverse_projection;
		Matrix inverse_view_projection;
		Matrix prev_view;
		Matrix prev_projection;
		Matrix prev_view_projection;
		Matrix reprojection;
		Matrix rain_view_projection;
		Vector3 camera_position;
		PAD;
		Vector3 camera_forward;
		PAD;
		float  camera_jitter_x;
		float  camera_jitter_y;
		float  camera_near;
		float  camera_far;

		Vector4 ambient_color;
		Vector4 wind_params;
		Vector4 sun_direction;
		Vector4 sun_color;
		Vector4 cascade_splits;

		float  display_resolution_x;
		float  display_resolution_y;
		float  render_resolution_x;
		float  render_resolution_y;

		float  mouse_normalized_coords_x;
		float  mouse_normalized_coords_y;
		float  delta_time;
		float  total_time;

		uint32 frame_count;
		int32  lights_idx;
		int32  lights_matrices_idx;
		int32  accel_struct_idx;
		int32  env_map_idx;
		int32  meshes_idx;
		int32  materials_idx;
		int32  instances_idx;
		int32  ddgi_volumes_idx;
		int32  printf_buffer_idx;

		int32  rain_splash_diffuse_idx;
		int32  rain_splash_bump_idx;
		int32  rain_blocker_map_idx;
		float  rain_total_time;
	};

	struct LightGPU
	{
		Vector4 position;
		Vector4 direction;
		Vector4 color;
		int32 active;
		float range;
		int32 type;
		float outer_cosine;

		float inner_cosine;
		bool32 volumetric;
		float volumetric_strength;
		bool32 use_cascades;

		int32 shadow_texture_index;
		int32 shadow_matrix_index;
		int32 shadow_mask_index;
		int32 padd;
	};

	struct MeshGPU
	{
		uint32 buffer_idx;
		uint32 positions_offset;
		uint32 uvs_offset;
		uint32 normals_offset;
		uint32 tangents_offset;
		uint32 indices_offset;
		uint32 meshlet_offset;
		uint32 meshlet_vertices_offset;
		uint32 meshlet_triangles_offset;
		uint32 meshlet_count;
	};

	struct MaterialGPU
	{
		uint32 diffuse_idx;
		uint32 normal_idx;
		uint32 roughness_metallic_idx;
		uint32 emissive_idx;
		Vector3 base_color_factor;
		float emissive_factor;
		float metallic_factor;
		float roughness_factor;
		float alpha_cutoff;
	};

	struct InstanceGPU
	{
		Matrix world_matrix;
		Matrix inverse_world_matrix;
		Vector3 bb_origin;
		PAD;
		Vector3 bb_extents;
		uint32 instance_id;
		uint32 material_idx;
		uint32 mesh_index;
		PAD[2];
	};
}