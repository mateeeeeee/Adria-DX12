#pragma once
#include <string>
#include "Utilities/Singleton.h"

namespace adria
{
	class CLIParser;
	enum class LogLevel : Uint8;

	class CommandLineOptions : public Singleton<CommandLineOptions>
	{
		friend class Singleton<CommandLineOptions>;
	public:
		void Initialize(std::wstring const& cmd_line);

		std::string const& GetLogFile() const { return log_file; }
		LogLevel GetLogLevel() const { return static_cast<LogLevel>(log_level); }
		std::string const& GetWindowTitle() const { return window_title; }
		Int GetWindowWidth() const { return window_width; }
		Int GetWindowHeight() const { return window_height; }
		Bool GetMaximizeWindow() const { return maximize_window; }
		std::string const& GetSceneFile() const { return scene_file; }
		Bool GetVsync() const { return vsync; }
		Bool GetDebugDevice() const { return debug_device; }
		Bool GetShaderDebug() const { return shader_debug; }
		Bool GetDRED() const { return dred; }
		Bool GetGpuValidation() const { return gpu_validation; }
		Bool GetPIX() const { return pix; }
		Bool GetAftermath() const { return aftermath; }

	private:
		std::string log_file;
		Int log_level;
		std::string window_title;
		Int window_width;
		Int window_height;
		Bool maximize_window;
		std::string scene_file;
		Bool vsync;
		Bool debug_device;
		Bool shader_debug;
		Bool dred;
		Bool gpu_validation;
		Bool pix;
		Bool aftermath;

	private:
		CommandLineOptions();
		void RegisterOptions(CLIParser&);
	};
	#define g_CommandLineOptions CommandLineOptions::Get()
}

