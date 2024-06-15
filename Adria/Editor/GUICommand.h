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
		GUICommandGroup_PostProcessing
	};
	static char const* GUICommandGroupNames[] =
	{
		"",
		"Renderer",
		"PostProcessing"
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
	void GUI_RunCommand(std::function<void()>&& cb, GUICommandGroup group = GUICommandGroup_None);

	class GfxTexture;
	struct GUITexture
	{
		char const* name;
		GfxTexture* gfx_texture;
	};
	void GUI_DisplayTexture(char const* name, GfxTexture* gfx_texture);
}