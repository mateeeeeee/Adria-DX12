#pragma once
#include "Definitions.h"
#include "Windows.h"
#include <functional>


namespace adria
{

	struct window_message_t
	{
		void* handle	= nullptr;
        void* instance  = nullptr;
		u32 msg			= 0;
        u64 wparam      = 0;
        i64 lparam      = 0;
		f32 width		= 0;
		f32 height		= 0;
	};

    struct window_init_t
    {
        HINSTANCE instance;
        std::string title;
        u32 width, height;
        bool maximize;
    };

    //move definitions to .cpp file
	class Window
	{
	public:
        static void Initialize(window_init_t const& init);

        static u32 Width();

        static u32 Height();

        static std::pair<u32, u32> Position();

        static void SetCallback(std::function<void(window_message_t const& window_data)> callback);

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        static bool Loop();

        static void Destroy();

        static void* Handle();

        static bool IsActive();

	private:
        inline static HINSTANCE _instance = nullptr;
        inline static HWND _handle = nullptr;
		inline static std::function<void(window_message_t const& window_data)> _msg_callback = nullptr;
	};
}