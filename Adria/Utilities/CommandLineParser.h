#pragma once
#include <shellapi.h>
#include <string>
#include <vector>
#include "../Utilities/StringUtil.h"
#include "../Core/Definitions.h"

namespace adria
{

	struct command_line_config_info_t
	{
		uint32 window_width = 1080;
		uint32 window_height = 720;
		std::string window_title = "adria";
		bool window_maximize = false;
		bool vsync = false;
		std::string scene_file = "scene.json";
		std::string log_file = "adria.log";
		int log_level = 0;
	};

	static command_line_config_info_t ParseCommandLine(LPWSTR command_line)
	{
		command_line_config_info_t config{};

		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(command_line, &argc);
		if (argv == NULL)
		{
			OutputDebugStringW(L"Cannot parse command line, returning default configuration\n");
			return config;
		}

		std::vector<std::wstring> args(argv, argv + argc);
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (args[i] == L"--scene")
			{
				config.scene_file = ConvertToNarrow(args[++i]);
			}
			else if (args[i] == L"--log")
			{
				config.log_file = ConvertToNarrow(args[++i]);
			}
			else if (args[i] == L"--loglevel")
			{
				config.log_level = _wtoi(args[++i].c_str());
			}
			else if (args[i] == L"--max")
			{
				config.window_maximize = true;
			}
			else if (args[i] == L"--vsync")
			{
				config.vsync = true;
			}
			else if (args[i] == L"--title")
			{
				config.window_title = _wtoi(args[++i].c_str());
			}
			else if (args[i] == L"--width")
			{
				config.window_width = _wtoi(args[++i].c_str());
			}
			else if (args[i] == L"--height")
			{
				config.window_height = _wtoi(args[++i].c_str());
			}
		}
		LocalFree(argv);
		return config;
	}
}