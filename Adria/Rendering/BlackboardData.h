#pragma once
#include "Core/CoreTypes.h"

namespace adria
{
	struct FrameBlackboardData
	{
		DirectX::XMMATRIX			camera_view;
		DirectX::XMMATRIX			camera_proj;
		DirectX::XMMATRIX			camera_viewproj;
		float						camera_fov;
		float						camera_position[4];
		uint64						frame_cbuffer_address;
	};

	struct DoFBlackboardData
	{
		float dof_params_x;
		float dof_params_y;
		float dof_params_z;
		float dof_params_w;
	};

	struct BloomBlackboardData
	{
		float bloom_intensity;
		float bloom_blend_factor;
	};
}