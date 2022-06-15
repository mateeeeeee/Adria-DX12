#pragma once
#include "../Core/Definitions.h"

namespace adria
{
	struct ViewportData
	{
		float32 scene_viewport_pos_x;
		float32 scene_viewport_pos_y;
		float32 scene_viewport_size_x;
		float32 scene_viewport_size_y;
		bool    scene_viewport_focused;
		float32 mouse_position_x;
		float32 mouse_position_y;
	};
}