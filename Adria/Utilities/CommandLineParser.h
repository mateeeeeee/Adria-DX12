#pragma once
#include <shellapi.h>
#include <string>
#include "../Core/Definitions.h"


namespace adria
{

	struct command_line_config_info_t
	{
		u32 window_width = 1080;
		u32 window_height = 720;
		std::string window_title = "adria";
		bool window_maximize = false;
		bool vsync = true;
		std::string log_file = "adria.log";
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

		if (argc > 0)
		{
			CHAR str[256];
			INT32 i = 0;
			while (i < argc)
			{
				wcstombs(str, argv[i], sizeof(str));

				if (strcmp(str, "-width") == 0)
				{
					i++;
					wcstombs(str, argv[i], sizeof(str));
					config.window_width = atoi(str);
				}
				else if (strcmp(str, "-height") == 0)
				{
					i++;
					wcstombs(str, argv[i], sizeof(str));
					config.window_height = atoi(str);
				}
				else if (strcmp(str, "-title") == 0)
				{
					i++;
					wcstombs(str, argv[i], sizeof(str));
					config.window_title = str;
				}
				else if (strcmp(str, "-max") == 0)
				{
					i++;
					wcstombs(str, argv[i], sizeof(str));
					if (strcmp(str, "true") == 0) config.window_maximize = true;
					else if (strcmp(str, "false") == 0) config.window_maximize = false;
					//else use default
				}
				else if (strcmp(str, "-vsync") == 0)
				{
					i++;
					wcstombs(str, argv[i], sizeof(str));
					if (strcmp(str, "true") == 0) config.vsync = true;
					else if (strcmp(str, "false") == 0) config.vsync = false;
					//else use default
				}
				else if (strcmp(str, "-log") == 0)
				{
					i++;
					wcstombs(str, argv[i], sizeof(str));
					config.log_file = str;
				}
				i++;
			}
		}

		LocalFree(argv);
		return config;
	}
}