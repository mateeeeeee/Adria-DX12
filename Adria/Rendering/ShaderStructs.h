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
		Sint32  lights_idx;
		Sint32  lights_matrices_idx;
		Sint32  accel_struct_idx;
		Sint32  env_map_idx;
		Sint32  meshes_idx;
		Sint32  materials_idx;
		Sint32  instances_idx;
		Sint32  ddgi_volumes_idx;
		Sint32  printf_buffer_idx;

		Sint32  rain_splash_diffuse_idx;
		Sint32  rain_splash_bump_idx;
		Sint32  rain_blocker_map_idx;
		Float  rain_total_time;
	};

	struct LightGPU
	{
		Vector4 position;
		Vector4 direction;
		Vector4 color;
		Sint32 active;
		Float range;
		Sint32 type;
		Float outer_cosine;

		Float inner_cosine;
		Bool32 volumetric;
		Float volumetric_strength;
		Bool32 use_cascades;

		Sint32 shadow_texture_index;
		Sint32 shadow_matrix_index;
		Sint32 shadow_mask_index;
		Sint32 padd;
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
		Uint32 diffuse_idx;
		Uint32 normal_idx;
		Uint32 roughness_metallic_idx;
		Uint32 emissive_idx;
		Vector3 base_color_factor;
		Float emissive_factor;
		Float metallic_factor;
		Float roughness_factor;
		Float alpha_cutoff;
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