#include "GUICommand.h"
#include "Editor.h"

namespace adria
{
	void GUI_RunCommand(std::function<void()>&& cb)
	{
		Editor::Get().AddCommand(GUICommand{ .callback = std::move(cb) });
	}

	void GUI_DisplayTexture(char const* name, GfxTexture* gfx_texture)
	{
		Editor::Get().AddDebugTexture(GUITexture{ .name = name, .gfx_texture = gfx_texture });
	}

}

