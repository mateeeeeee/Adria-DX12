#pragma once
#include <functional>
#include <stdarg.h>

namespace adria
{
	struct GUICommand
	{
		std::function<void()> gui_callback;
	};

	inline void AddGUI(int x)
	{
		
	}
	
}