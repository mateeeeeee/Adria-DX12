#pragma once

#ifndef DECLSPEC_ALIGN
#define DECLSPEC_ALIGN(x)   __declspec(align(x))
#endif

#define PAD Uint32 ADRIA_CONCAT(pad, __COUNTER__)

namespace adria
{
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
		Float  camera_jitter_x;
		Float  camera_jitter_y;
		Float  camera_near;
		Float  camera_far;

		Vector4 ambient_color;
		Vector4 wind_params;
		Vector4 sun_direction;
		Vector4 sun_color;
		Vector4 cascade_splits;

		Float  display_resolution_x;
		Float  display_resolution_y;
		Float  render_resolution_x;
		Float  render_resolution_y;

		Float  mouse_normalized_coords_x;
		Float  mouse_normalized_coords_y;
		Float  delta_time;
		Float  total_time;

		Uint32 frame_count;
		Int32  lights_idx;
		Int32  light_count;
		Int32  lights_matrices_idx;
		Int32  accel_struct_idx;
		Int32  env_map_idx;
		Int32  meshes_idx;
		Int32  materials_idx;
		Int32  instances_idx;
		Int32  ddgi_volumes_idx;
		Int32  printf_buffer_idx;

		Int32  rain_splash_diffuse_idx;
		Int32  rain_splash_bump_idx;
		Int32  rain_blocker_map_idx;
		Int32  sheenE_idx;
		Int32  triangle_overdraw_idx;
		Float  rain_total_time;
	};

	struct LightGPU
	{
		Vector4 position;
		Vector4 direction;
		Vector4 color;
		Int32 active;
		Float range;
		Int32 type;
		Float outer_cosine;

		Float inner_cosine;
		Bool32 volumetric;
		Float volumetric_strength;
		Bool32 use_cascades;

		Int32 shadow_texture_index;
		Int32 shadow_matrix_index;
		Int32 shadow_mask_index;
		Int32 padd;
	};

	struct MeshGPU
	{
		Uint32 buffer_idx;
		Uint32 positions_offset;
		Uint32 uvs_offset;
		Uint32 normals_offset;
		Uint32 tangents_offset;
		Uint32 indices_offset;
		Uint32 meshlet_offset;
		Uint32 meshlet_vertices_offset;
		Uint32 meshlet_triangles_offset;
		Uint32 meshlet_count;
	};

	struct MaterialGPU
	{
		Uint32  shading_extension;
		Vector3 albedo_color;
		Uint32  albedo_idx;
		Uint32  roughness_metallic_idx;
		Float   metallic_factor;
		Float   roughness_factor;

		Uint32  normal_idx;
		Uint32  emissive_idx;
		Float   emissive_factor;
		Float   alpha_cutoff;

		Int32	anisotropy_idx;
		Float	anisotropy_strength;
		Float	anisotropy_rotation;

		Uint32 clear_coat_idx;
		Uint32 clear_coat_roughness_idx;
		Uint32 clear_coat_normal_idx;
		Float  clear_coat;
		Float  clear_coat_roughness;

		Vector3 sheen_color;
		Float sheen_roughness;
		Uint32 sheen_color_idx;
		Uint32 sheen_roughness_idx;
	};

	struct InstanceGPU
	{
		Matrix world_matrix;
		Matrix inverse_world_matrix;
		Vector3 bb_origin;
		PAD;
		Vector3 bb_extents;
		Uint32 instance_id;
		Uint32 material_idx;
		Uint32 mesh_index;
		PAD[2];
	};
}