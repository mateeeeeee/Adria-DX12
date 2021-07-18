#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")


#define STB_IMAGE_IMPLEMENTATION  
#include <stb_image.h>
#include "Utilities/MemoryLeak.h"
#include "Core/Window.h"
#include "Core/Engine.h"
#include "Editor/Editor.h"

using namespace adria;

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    /*
    MemoryLeak::SetBreak(5583);
    752 bytes of "memory leak" is because of FileDialog singleton. TODO: replace singleton usage with
    instance usage to avoid false memory check
    */
    //MemoryLeak::Checkpoint();
    {
        window_init_t window_init{};
        window_init.instance = hInstance;
        window_init.width = 1080;
        window_init.height = 720;
        window_init.title = "Adria";
        window_init.maximize = true;
        Window::Initialize(window_init);

        engine_init_t engine_init{};
        engine_init.vsync = false;
        engine_init.load_default_scene = true;

        editor_init_t editor_init{};
        editor_init.engine_init = std::move(engine_init);
        editor_init.log_file = "adria.log";
        Editor editor{ editor_init };

        Window::SetCallback([&editor](window_message_t const& msg_data) {editor.HandleWindowMessage(msg_data); });

        while (Window::Loop())
        {
            editor.Run();
        }
        Window::Destroy();
    }
    //MemoryLeak::CheckLeaks();

}



