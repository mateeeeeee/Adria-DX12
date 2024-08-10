#pragma once
#include <functional>
#include <string>
#include "ImGuiManager.h"

namespace adria
{
	enum GUICommandGroup
	{
		GUICommandGroup_None,
		GUICommandGroup_Renderer,
		GUICommandGroup_PostProcessing,
		GUICommandGroup_Count
	};
	static char const* GUICommandGroupNames[] =
	{
		"",
		"Renderer",
		"Postprocessing"
	};
	enum GUICommandSubGroup
	{
		GUICommandSubGroup_AO,
		GUICommandSubGroup_None,
		GUICommandSubGroup_Reflection,
		GUICommandSubGroup_DepthOfField,
		GUICommandSubGroup_Upscaler,
		GUICommandSubGroup_Antialiasing,
		GUICommandSubGroup_Count
	};
	static char const* GUICommandSubGroupNames[] =
	{
		"Ambient Occlusion",
		"",
		"Reflection",
		"Depth Of Field",
		"Upscaler",
		"Antialiasing"
		""
	};

	struct GUICommand
	{
		std::function<void()> callback;
		GUICommandGroup group;
		GUICommandSubGroup subgroup;
	};
	void QueueGUI(std::function<void()>&& cb, GUICommandGroup group = GUICommandGroup_None, GUICommandSubGroup = GUICommandSubGroup_None);

	class GfxTexture;
	struct GUITexture
	{
		char const* name;
		GfxTexture* gfx_texture;
	};
	void GUI_DebugTexture(char const* name, GfxTexture* gfx_texture);
}