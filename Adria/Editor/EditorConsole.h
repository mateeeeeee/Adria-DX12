#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGui/imgui.h"

namespace adria
{
	class EditorConsole
	{
	public:
		EditorConsole();
		~EditorConsole();

		void Draw(const Char* title, Bool* p_open = nullptr);
		void DrawBasic(const Char* title, Bool* p_open = nullptr);

	private:
		Char                  InputBuf[256];
		ImVector<Char*>       Items;
		ImVector<const Char*> Commands;
		ImVector<const Char*> CommandDescriptions;
		ImVector<Char*>       History;
		Int                   HistoryPos;
		ImGuiTextFilter       Filter;
		Bool                  AutoScroll;
		Bool                  ScrollToBottom;
		Int					  CursorPos;

	private:
		void    ClearLog();
		void    AddLog(const Char* fmt, ...) IM_FMTARGS(2);

		void ExecCommand(const Char* cmd);
		int	 TextEditCallback(ImGuiInputTextCallbackData* data);
	};
}

