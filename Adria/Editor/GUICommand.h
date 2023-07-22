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
	void GUI_RunCommand(std::function<void()>&& cb);

	class GfxTexture;
	struct GUITexture
	{
		char const* name;
		GfxTexture* gfx_texture;
	};
	void GUI_DisplayTexture(char const* name, GfxTexture* gfx_texture);
}