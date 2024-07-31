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

		void Draw(const char* title, bool* p_open = nullptr);
		void DrawBasic(const char* title, bool* p_open = nullptr);

	private:
		char                  InputBuf[256];
		ImVector<char*>       Items;
		ImVector<const char*> Commands;
		ImVector<const char*> CommandDescriptions;
		ImVector<char*>       History;
		int                   HistoryPos;
		ImGuiTextFilter       Filter;
		bool                  AutoScroll;
		bool                  ScrollToBottom;
		int					  CursorPos;

	private:
		void    ClearLog();
		void    AddLog(const char* fmt, ...) IM_FMTARGS(2);

		void ExecCommand(const char* cmd);
		int	 TextEditCallback(ImGuiInputTextCallbackData* data);
	};
}

