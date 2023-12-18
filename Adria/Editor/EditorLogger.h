#pragma once
#include <memory>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGui/imgui.h"
#include "Logging/Logger.h"

namespace adria
{
	class EditorLogger : public ILogger
	{
	public:
		EditorLogger(LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32_t line) override;
		void Draw(const char* title, bool* p_open = nullptr);
	private:
		std::unique_ptr<struct ImGuiLogger> imgui_log;
		LogLevel logger_level;
	};

}