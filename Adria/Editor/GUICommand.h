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
	static Char const* GUICommandGroupNames[] =
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
	static Char const* GUICommandSubGroupNames[] =
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
		Char const* name;
		GfxTexture* gfx_texture;
	};
	void GUI_DebugTexture(Char const* name, GfxTexture* gfx_texture);
}