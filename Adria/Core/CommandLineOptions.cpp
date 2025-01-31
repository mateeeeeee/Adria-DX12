#include "CommandLineOptions.h"
#include "Utilities/CLIParser.h"

namespace adria
{
	void CommandLineOptions::Initialize(std::wstring const& cmd_line)
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
	}

	CommandLineOptions::CommandLineOptions()
		: log_file{}, log_level(0), window_title{}, window_width(0), window_height(0), maximize_window(false), scene_file(""),
		vsync(false), debug_device(false), shader_debug(false), dred(false), gpu_validation(false),
		pix(false), aftermath(false)
	{
	}

	void CommandLineOptions::RegisterOptions(CLIParser& cli_parser)
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

