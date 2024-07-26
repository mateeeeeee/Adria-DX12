#pragma once

namespace adria
{
	struct FrameBlackboardData
	{
		DirectX::XMMATRIX			camera_view;
		DirectX::XMMATRIX			camera_proj;
		DirectX::XMMATRIX			camera_viewproj;
		float						camera_fov;
		float						camera_aspect_ratio;
		float						camera_near;
		float						camera_far;
		float						camera_jitter_x;
		float						camera_jitter_y;
		float						camera_position[4];
		float						delta_time;
		uint64						frame_cbuffer_address;
	};

	struct DoFBlackboardData
	{
		float dof_focus_distance;
		float dof_focus_radius;
	};

	struct BloomBlackboardData
	{
		float bloom_intensity;
		float bloom_blend_factor;
	};
}