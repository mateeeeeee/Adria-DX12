#include "GUICommand.h"
#include "Editor.h"

namespace adria
{
	void GUI_RunCommand(std::function<void()>&& cb, GUICommandGroup group)
	{
		g_Editor.AddCommand(GUICommand{ .callback = std::move(cb), .group = group });
	}

	void GUI_DisplayTexture(char const* name, GfxTexture* gfx_texture)
	{
		g_Editor.AddDebugTexture(GUITexture{ .name = name, .gfx_texture = gfx_texture });
	}

}

