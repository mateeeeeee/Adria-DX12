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
	CLIArg& width = parser.AddArg(true, "-w", "--width");
	CLIArg& height = parser.AddArg(true, "-h", "--height");
	CLIArg& title = parser.AddArg(true, "-title");
	CLIArg& config = parser.AddArg(true, "-cfg", "--config");
	CLIArg& scene = parser.AddArg(true, "-scene", "--scenefile");
	CLIArg& log = parser.AddArg(true, "-log", "--logfile");
	CLIArg& loglevel = parser.AddArg(true, "-loglvl", "--loglevel");
	CLIArg& maximize = parser.AddArg(false, "-max", "--maximize");
	CLIArg& vsync = parser.AddArg(false, "-vsync");
	CLIArg& debug_layer = parser.AddArg(false, "-debug_layer");
	CLIArg& dred_debug = parser.AddArg(false, "-dred_debug");
	CLIArg& gpu_validation = parser.AddArg(false, "-gpu_validation");
	CLIArg& pix = parser.AddArg(false, "-pix");

	parser.Parse(lpCmdLine);
    //MemoryDebugger::SetAllocHook(MemoryAllocHook);
    {
        std::string log_file = log.AsStringOr("adria.log");
        LogLevel log_level = static_cast<LogLevel>(loglevel.AsIntOr(0));
        std::unique_ptr<FileLogger> file_logger = std::make_unique<FileLogger>(log_file.c_str(), log_level);
        std::unique_ptr<OutputDebugStringLogger> output_debug_logger = std::make_unique<OutputDebugStringLogger>(log_level);
        REGISTER_LOGGER(file_logger.get());
        REGISTER_LOGGER(output_debug_logger.get());

		std::string title_str = title.AsStringOr("Adria").c_str();
        WindowInit window_init{};
        window_init.instance = hInstance;
        window_init.width = width.AsIntOr(1080);
        window_init.height = height.AsIntOr(720);
        window_init.title = title_str.c_str();
        window_init.maximize = maximize;
        Window window(window_init);
        g_Input.Initialize(&window);

        EngineInit engine_init{};
        engine_init.vsync = vsync;
        engine_init.debug_layer = debug_layer;
        engine_init.dred = dred_debug;
        engine_init.gpu_validation = gpu_validation;
        engine_init.pix = pix;
        engine_init.scene_file = scene.AsStringOr("scene.json");
		engine_init.window = &window;

        EditorInit editor_init{.engine_init = engine_init };
        g_Editor.Init(std::move(editor_init));

        window.GetWindowEvent().Add([](WindowEventData const& msg_data) {g_Editor.OnWindowEvent(msg_data); });
        while (window.Loop())
        {
            g_Editor.Run();
        }
        g_Editor.Destroy();
    }
}



