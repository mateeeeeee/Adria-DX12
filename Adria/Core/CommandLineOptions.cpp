#include "CommandLineOptions.h"
#include "Utilities/CLIParser.h"

namespace adria::CommandLineOptions
{
	namespace
	{
		std::string log_file{};
		Int log_level = 0;
		std::string window_title{};
		Int window_width = 0;
		Int window_height = 0;
		Bool maximize_window = false;
		std::string scene_file{};
		Bool vsync = false;
		Bool debug_device = false;
		Bool shader_debug = false;
		Bool dred = false;
		Bool gpu_validation = false;
		Bool pix = false;
		Bool aftermath = false;

		void RegisterOptions(CLIParser& cli_parser)
		{
			cli_parser.AddArg(true, "-w", "--width");
			cli_parser.AddArg(true, "-h", "--height");
			cli_parser.AddArg(true, "-title");
			cli_parser.AddArg(true, "-cfg", "--config");
			cli_parser.AddArg(true, "-scene", "--scenefile");
			cli_parser.AddArg(true, "-log", "--logfile");
			cli_parser.AddArg(true, "-loglvl", "--loglevel");
			cli_parser.AddArg(false, "-max", "--maximize");
			cli_parser.AddArg(false, "-vsync");
			cli_parser.AddArg(false, "-debugdevice");
			cli_parser.AddArg(false, "-shaderdebug");
			cli_parser.AddArg(false, "-dred");
			cli_parser.AddArg(false, "-gpuvalidation");
			cli_parser.AddArg(false, "-pix");
			cli_parser.AddArg(false, "-aftermath");
		}
	}

	void Initialize(std::wstring const& cmd_line)
	{
		CLIParser cli_parser{};
		RegisterOptions(cli_parser);
		CLIParseResult cli_result = cli_parser.Parse(cmd_line);
		
		log_file = cli_result["-log"].AsStringOr("adria.log");
		log_level = cli_result["-loglvl"].AsIntOr(0);
		window_title = cli_result["-title"].AsStringOr("Adria");
		window_width = cli_result["-w"].AsIntOr(1280);
		window_height = cli_result["-h"].AsIntOr(1024);
		maximize_window = cli_result["-max"];
		scene_file = cli_result["-scene"].AsStringOr("sponza.json");
		vsync = cli_result["-vsync"];
		debug_device = cli_result["-debugdevice"];
		shader_debug = cli_result["-shaderdebug"];
		dred = cli_result["-dred"];
		gpu_validation = cli_result["-gpuvalidation"];
		pix = cli_result["-pix"];
		aftermath = cli_result["-aftermath"];
	}

	std::string const& GetLogFile()
	{
		return log_file;
	}

	Int GetLogLevel()
	{
		return log_level;
	}

	std::string const& GetWindowTitle()
	{
		return window_title;
	}

	Int GetWindowWidth()
	{
		return window_width;
	}

	Int GetWindowHeight()
	{
		return window_height;
	}

	Bool GetMaximizeWindow()
	{
		return maximize_window;
	}

	std::string const& GetSceneFile()
	{
		return scene_file;
	}

	Bool GetVsync()
	{
		return vsync;
	}

	Bool GetDebugDevice()
	{
		return debug_device;
	}

	Bool GetShaderDebug()
	{
		return shader_debug;
	}

	Bool GetDRED()
	{
		return dred;
	}

	Bool GetGpuValidation()
	{
		return gpu_validation;
	}

	Bool GetPIX()
	{
		return pix;
	}

	Bool GetAftermath()
	{
		return aftermath;
	}

}

