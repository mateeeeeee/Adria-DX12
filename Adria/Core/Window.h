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
		U32 msg			= 0;
        U64 wparam      = 0;
        I64 lparam      = 0;
		F32 width		= 0;
		F32 height		= 0;
	};

    struct window_init_t
    {
        HINSTANCE instance;
        std::string title;
        U32 width, height;
        bool maximize;
    };

    //move definitions to .cpp file
	class Window
	{
	public:
        static void Initialize(window_init_t const& init);

        static U32 Width();

        static U32 Height();

        static std::pair<U32, U32> Position();

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