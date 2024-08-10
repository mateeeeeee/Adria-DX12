#include "GUICommand.h"
#include "Editor.h"

namespace adria
{
	void QueueGUI(std::function<void()>&& cb, GUICommandGroup group, GUICommandSubGroup subgroup)
	{
		g_Editor.AddCommand(GUICommand{ .callback = std::move(cb), .group = group, .subgroup = subgroup });
	}

	void GUI_DebugTexture(char const* name, GfxTexture* gfx_texture)
	{
		g_Editor.AddDebugTexture(GUITexture{ .name = name, .gfx_texture = gfx_texture });
	}

}

