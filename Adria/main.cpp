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
    CommandLineConfigInfo cmd_line_info = ParseCommandLine(lpCmdLine);
    {
        ADRIA_REGISTER_LOGGER(new FileLogger(cmd_line_info.log_file.c_str(), static_cast<ELogLevel>(cmd_line_info.log_level)));
        ADRIA_REGISTER_LOGGER(new OutputDebugStringLogger(static_cast<ELogLevel>(cmd_line_info.log_level)));

        window_init_t window_init{};
        window_init.instance = hInstance;
        window_init.width = cmd_line_info.window_width;
        window_init.height = cmd_line_info.window_height;
        window_init.title = cmd_line_info.window_title;
        window_init.maximize = cmd_line_info.window_maximize;
        Window::Initialize(window_init);

        engine_init_t engine_init{};
        engine_init.vsync = cmd_line_info.vsync;
        engine_init.scene_file = cmd_line_info.scene_file.c_str();

        editor_init_t editor_init{};
        editor_init.engine_init = std::move(engine_init);

        Editor editor{ editor_init };

        Window::SetCallback([&editor](window_message_t const& msg_data) {editor.HandleWindowMessage(msg_data); });

        while (Window::Loop())
        {
            editor.Run();
        }
        Window::Destroy();   
    }
}



