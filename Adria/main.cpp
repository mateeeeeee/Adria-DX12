#include "Core/Window.h"
#include "Core/Engine.h"
#include "Core/Input.h"
#include "Core/CommandLineOptions.h"
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
    g_CommandLineOptions.Initialize(lpCmdLine);
    
    std::string log_file = g_CommandLineOptions.GetLogFile();  
    LogLevel log_level = static_cast<LogLevel>(g_CommandLineOptions.GetLogLevel());
    g_Log.Register(new FileLogger(log_file.c_str(), log_level));
    g_Log.Register(new OutputDebugStringLogger(log_level));

    std::string window_title = g_CommandLineOptions.GetWindowTitle();
    WindowInit window_init{};
    window_init.width = g_CommandLineOptions.GetWindowWidth();
    window_init.height = g_CommandLineOptions.GetWindowHeight();
    window_init.title = window_title.c_str();
    window_init.maximize = g_CommandLineOptions.GetMaximizeWindow();
    Window window(window_init);
    g_Input.Initialize(&window);

    EditorInit editor_init{ .window = &window, .scene_file = g_CommandLineOptions.GetSceneFile() };
    g_Editor.Init(std::move(editor_init));
    window.GetWindowEvent().AddLambda([](WindowEventData const& msg_data) { g_Editor.OnWindowEvent(msg_data); });
    while (window.Loop())
    {
        g_Editor.Run();
    }
    g_Editor.Destroy();
    
}



