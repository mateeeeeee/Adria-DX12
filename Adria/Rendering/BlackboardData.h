#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "../Core/Definitions.h"

namespace adria
{
	struct FrameBlackboardData
	{
		DirectX::XMVECTOR			camera_position;
		DirectX::XMMATRIX			camera_view;
		DirectX::XMMATRIX			camera_proj;
		DirectX::XMMATRIX			camera_viewproj;
		float						camera_fov;
		D3D12_GPU_VIRTUAL_ADDRESS   frame_cbuffer_address;
	};

	struct DoFBlackboardData
	{
		float dof_params_x;
		float dof_params_y;
		float dof_params_z;
		float dof_params_w;
	};
}