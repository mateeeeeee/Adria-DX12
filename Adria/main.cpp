#include "Core/Window.h"
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Logging/FileLogger.h"
#include "Logging/OutputDebugStringLogger.h"
#include "Editor/Editor.h"
#include "Utilities/MemoryDebugger.h"
#include "Utilities/CLIParser.h"

using namespace adria;

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    CLIParser cli_parser{};
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
    CLIParseResult cli_result = cli_parser.Parse(lpCmdLine);
    
    std::string log_file = cli_result["-log"].AsStringOr("adria.log");
    LogLevel log_level = static_cast<LogLevel>(cli_result["-loglvl"].AsIntOr(0));
    g_Log.Register(new FileLogger(log_file.c_str(), log_level));
    g_Log.Register(new OutputDebugStringLogger(log_level));

	std::string title_str = cli_result["-title"].AsStringOr("Adria").c_str();
    WindowInit window_init{};
    window_init.width = cli_result["-w"].AsIntOr(1280);
    window_init.height = cli_result["-h"].AsIntOr(1024);
    window_init.title = title_str.c_str();
    window_init.maximize = cli_result["-max"];
    Window window(window_init);
    g_Input.Initialize(&window);

    EngineInit engine_init{};
    engine_init.scene_file = cli_result["-scene"].AsStringOr("pica_pica.json");
	engine_init.window = &window;
	engine_init.gfx_options.vsync = cli_result["-vsync"];
	engine_init.gfx_options.debug_device = cli_result["-debugdevice"];
	engine_init.gfx_options.shader_debug = cli_result["-shaderdebug"];
	engine_init.gfx_options.dred = cli_result["-dred"];
	engine_init.gfx_options.gpu_validation = cli_result["-gpuvalidation"];
	engine_init.gfx_options.pix = cli_result["-pix"];
	engine_init.gfx_options.aftermath = cli_result["-aftermath"];

    EditorInit editor_init{ .engine_init = engine_init };
    g_Editor.Init(std::move(editor_init));

    window.GetWindowEvent().AddLambda([](WindowEventData const& msg_data) { g_Editor.OnWindowEvent(msg_data); });
    while (window.Loop())
    {
        g_Editor.Run();
    }
    g_Editor.Destroy();
    
}



