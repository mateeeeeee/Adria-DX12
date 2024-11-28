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
    CLIParser parser{};
    {
		parser.AddArg(true, "-w", "--width");
		parser.AddArg(true, "-h", "--height");
		parser.AddArg(true, "-title");
		parser.AddArg(true, "-cfg", "--config");
		parser.AddArg(true, "-scene", "--scenefile");
		parser.AddArg(true, "-log", "--logfile");
		parser.AddArg(true, "-loglvl", "--loglevel");
		parser.AddArg(false, "-max", "--maximize");
		parser.AddArg(false, "-vsync");
		parser.AddArg(false, "-debugdevice");
		parser.AddArg(false, "-shaderdebug");
		parser.AddArg(false, "-dred");
		parser.AddArg(false, "-gpuvalidation");
		parser.AddArg(false, "-pix");
		parser.AddArg(false, "-aftermath");
    }
	parser.Parse(lpCmdLine);
    
    std::string log_file = parser["-log"].AsStringOr("adria.log");
    LogLevel log_level = static_cast<LogLevel>(parser["-loglvl"].AsIntOr(0));
    g_Log.Register(new FileLogger(log_file.c_str(), log_level));
    g_Log.Register(new OutputDebugStringLogger(log_level));

	std::string title_str = parser["-title"].AsStringOr("Adria").c_str();
    WindowInit window_init{};
    window_init.width = parser["-w"].AsIntOr(1280);
    window_init.height = parser["-h"].AsIntOr(1024);
    window_init.title = title_str.c_str();
    window_init.maximize = parser["-max"];
    Window window(window_init);
    g_Input.Initialize(&window);

    EngineInit engine_init{};
    engine_init.scene_file = parser["-scene"].AsStringOr("sponza.json");
	engine_init.window = &window;
	engine_init.gfx_options.vsync = parser["-vsync"];
	engine_init.gfx_options.debug_device = parser["-debugdevice"];
	engine_init.gfx_options.shader_debug = parser["-shaderdebug"];
	engine_init.gfx_options.dred = parser["-dred"];
	engine_init.gfx_options.gpu_validation = parser["-gpuvalidation"];
	engine_init.gfx_options.pix = parser["-pix"];
	engine_init.gfx_options.aftermath = parser["-aftermath"];

    EditorInit editor_init{ .engine_init = engine_init };
    g_Editor.Init(std::move(editor_init));

    window.GetWindowEvent().AddLambda([](WindowEventData const& msg_data) { g_Editor.OnWindowEvent(msg_data); });
    while (window.Loop())
    {
        g_Editor.Run();
    }
    g_Editor.Destroy();
    
}



