#pragma once
#include <memory>
#include "ImGui/imgui.h"
#include "Logging/Logger.h"

namespace adria
{
	class EditorLogger : public ILogger
	{
	public:
		EditorLogger(LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32_t line) override;
		void Draw(const char* title, bool* p_open = NULL);
	private:
		std::unique_ptr<struct ImGuiLogger> imgui_log;
		LogLevel logger_level;
	};

	class EditorConsole
	{
	public:
		EditorConsole();
		~EditorConsole();

		void Draw(const char* title);

	private:
		char                  InputBuf[256];
		ImVector<char*>       Items;
		ImVector<const char*> Commands;
		ImVector<char*>       History;
		int                   HistoryPos;   
		ImGuiTextFilter       Filter;
		bool                  AutoScroll;
		bool                  ScrollToBottom;

	private:
		void    ClearLog();
		void    AddLog(const char* fmt, ...) IM_FMTARGS(2);

		void ExecCommand(const char* cmd);
		int	TextEditCallback(ImGuiInputTextCallbackData* data);

		static int   Stricmp(const char* s1, const char* s2);
		static int   Strnicmp(const char* s1, const char* s2, int n);
		static char* Strdup(const char* s);
		static void  Strtrim(char* s);
	};
}