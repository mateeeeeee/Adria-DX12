#pragma once
#include "Definitions.h"
#include "Windows.h"
#include <functional>


namespace adria
{

	struct WindowMessage
	{
		void* handle	= nullptr;
        void* instance  = nullptr;
		uint32 msg			= 0;
        uint64 wparam      = 0;
        int64 lparam      = 0;
		float32 width		= 0;
		float32 height		= 0;
	};

    struct WindowInit
    {
        HINSTANCE instance;
        std::string title;
        uint32 width, height;
        bool maximize;
    };

    //move definitions to .cpp file
	class Window
	{
	public:
        static void Initialize(WindowInit const& init);

        static uint32 Width();

        static uint32 Height();

        static std::pair<uint32, uint32> Position();

        static void SetCallback(std::function<void(WindowMessage const& window_data)> callback);

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        static bool Loop();

        static void Quit(int exit_code);

        static void Destroy();

        static void* Handle();

        static bool IsActive();

	private:
        inline static HINSTANCE _instance = nullptr;
        inline static HWND _handle = nullptr;
		inline static std::function<void(WindowMessage const& window_data)> _msg_callback = nullptr;
	};
}