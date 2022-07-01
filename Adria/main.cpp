#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#define STB_IMAGE_IMPLEMENTATION  
#include <stb_image.h>
#include "Core/Window.h"
#include "Core/Engine.h"
#include "Logging/Logger.h"
#include "Editor/Editor.h"
#include "Utilities/MemoryDebugger.h"
#include "Utilities/CommandLineParser.h"

using namespace adria;

int MemoryAllocHook(int allocType, void* userData, std::size_t size, int blockType, long requestNumber,
	const unsigned char* filename, int lineNumber)
{
    return 1;
}

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    //MemoryDebugger::SetAllocHook(MemoryAllocHook);
    CommandLineArgs::Parse(lpCmdLine);
    {
        char const* log_file = CommandLineArgs::GetString(ECmdLineArg::LogFile);
        int32 log_level = CommandLineArgs::GetInt(ECmdLineArg::LogLevel);
        ADRIA_REGISTER_LOGGER(new FileLogger(log_file, static_cast<ELogLevel>(log_level)));
        ADRIA_REGISTER_LOGGER(new OutputDebugStringLogger(static_cast<ELogLevel>(log_level)));

        WindowInit window_init{};
        window_init.instance = hInstance;
        window_init.width = CommandLineArgs::GetInt(ECmdLineArg::WindowWidth);
        window_init.height = CommandLineArgs::GetInt(ECmdLineArg::WindowHeight);
        window_init.title = CommandLineArgs::GetString(ECmdLineArg::WindowTitle);
        window_init.maximize = CommandLineArgs::GetBool(ECmdLineArg::WindowMaximize);
        Window::Initialize(window_init);

        EngineInit engine_init{};
        engine_init.vsync = CommandLineArgs::GetBool(ECmdLineArg::Vsync);
        engine_init.debug_layer = CommandLineArgs::GetBool(ECmdLineArg::DebugLayer);
        engine_init.dred = CommandLineArgs::GetBool(ECmdLineArg::DredDebug);
        engine_init.gpu_validation = CommandLineArgs::GetBool(ECmdLineArg::GpuValidation);
        engine_init.scene_file = CommandLineArgs::GetString(ECmdLineArg::SceneFile);

        EditorInit editor_init{};
        editor_init.engine_init = std::move(engine_init);

        Editor editor{ editor_init };

        Window::SetCallback([&editor](WindowMessage const& msg_data) {editor.HandleWindowMessage(msg_data); });

        while (Window::Loop())
        {
            editor.Run();
        }
        Window::Destroy();   
    }
}



