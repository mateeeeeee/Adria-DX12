#include "GUICommand.h"
#include "Editor.h"

namespace adria
{
	void AddGUI(std::function<void()>&& cb)
	{
		Editor::GetInstance().AddCommand(GUICommand{ .callback = std::move(cb) });
	}

	void AddGUI_Debug(std::function<void(void*)>&& cb)
	{
		Editor::GetInstance().AddDebugCommand(GUICommand_Debug{ .callback = std::move(cb) });
	}

}

