#include "GUICommand.h"
#include "Editor.h"

namespace adria
{
	void AddGUI(std::function<void()>&& cb)
	{
		Editor::Get().AddCommand(GUICommand{ .callback = std::move(cb) });
	}
}

