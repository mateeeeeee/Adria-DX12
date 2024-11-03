#pragma once

namespace adria
{
	struct FrameBlackboardData
	{
		DirectX::XMMATRIX			camera_view;
		DirectX::XMMATRIX			camera_proj;
		DirectX::XMMATRIX			camera_viewproj;
		Float						camera_fov;
		Float						camera_aspect_ratio;
		Float						camera_near;
		Float						camera_far;
		Float						camera_jitter_x;
		Float						camera_jitter_y;
		Float						camera_position[4];
		Float						delta_time;
		Uint64						frame_cbuffer_address;
	};

	struct DoFBlackboardData
	{
		Float dof_focus_distance;
		Float dof_focus_radius;
	};

	struct BloomBlackboardData
	{
		Float bloom_intensity;
		Float bloom_blend_factor;
	};
}