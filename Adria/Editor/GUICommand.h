#pragma once
#include <functional>
#include "GUI.h"

namespace adria
{
	struct GUICommand
	{
		std::function<void()> callback;
	};
	void AddGUI(std::function<void()>&& cb);
}