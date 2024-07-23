#pragma once
#include <functional>
#include <string>
#include "GUI.h"

namespace adria
{
	enum GUICommandGroup
	{
		GUICommandGroup_None,
		GUICommandGroup_Renderer,
		GUICommandGroup_PostProcessor,
		GUICommandGroup_Count
	};
	static char const* GUICommandGroupNames[] =
	{
		"",
		"Renderer Settings",
		"Post-processing Settings"
	};

	struct GUICommand
	{
		std::function<void()> callback;
		GUICommandGroup group;
		void operator()() const
		{
			callback();
		}
	};
	void GUI_Command(std::function<void()>&& cb, GUICommandGroup group = GUICommandGroup_None);

	class GfxTexture;
	struct GUITexture
	{
		char const* name;
		GfxTexture* gfx_texture;
	};
	void GUI_DisplayTexture(char const* name, GfxTexture* gfx_texture);
}