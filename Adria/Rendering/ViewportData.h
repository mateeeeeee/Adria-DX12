#pragma once
#include "Core/CoreTypes.h"

namespace adria
{
	struct ViewportData
	{
		float scene_viewport_pos_x;
		float scene_viewport_pos_y;
		float scene_viewport_size_x;
		float scene_viewport_size_y;
		bool  scene_viewport_focused;
		float mouse_position_x;
		float mouse_position_y;
	};
}