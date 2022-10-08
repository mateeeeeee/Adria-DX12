#include "GUICommand.h"
#include "Editor.h"

namespace adria
{
	void AddGUI(std::function<void()>&& cb)
	{
		Editor::GetInstance().AddCommand(GUICommand{ .callback = std::move(cb) });
	}
}

