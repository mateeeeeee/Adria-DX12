#pragma once
#include <string>

namespace adria
{
	namespace CommandLineOptions
	{
		void Initialize(std::wstring const& cmd_line);

		std::string const& GetLogFile();
		Int GetLogLevel();
		std::string const& GetWindowTitle();
		Int GetWindowWidth();
		Int GetWindowHeight();
		Bool GetMaximizeWindow();
		std::string const& GetSceneFile();
		Bool GetVsync();
		Bool GetDebugDevice();
		Bool GetShaderDebug();
		Bool GetDRED();
		Bool GetGpuValidation();
		Bool GetPIX();
		Bool GetAftermath();
		Bool GetPerfReport();
		Bool GetPerfHUD();
	}
}

