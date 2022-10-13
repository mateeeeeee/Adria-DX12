#pragma once
#include <functional>
#include "GUI.h"

namespace adria
{
	struct GUICommand
	{
		std::function<void()> callback;
		void operator()() const
		{
			callback();
		}
	};
	struct GUICommand_Debug
	{
		std::function<void(void*)> callback;
		void operator()(void* args = nullptr) const
		{
			callback(args);
		}
	};

	void AddGUI(std::function<void()>&& cb);
	void AddGUI_Debug(std::function<void(void*)>&& cb);
}