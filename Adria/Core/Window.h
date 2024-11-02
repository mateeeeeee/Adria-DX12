#pragma once
#include "Windows.h"
#include "Utilities/Delegate.h"

namespace adria
{
	struct WindowEventData
	{
		void*  handle	= nullptr;
		Uint32 msg		= 0;
        Uint64 wparam	= 0;
        Sint64  lparam	= 0;
		float  width	= 0.0f;
		float  height	= 0.0f;
	};

    struct WindowInit
    {
        char const* title;
        Uint32 width, height;
        bool maximize;
    };

	DECLARE_EVENT(WindowEvent, Window, WindowEventData const&)

	class Window
	{
	public:
		Window(WindowInit const& init);
		~Window();

		Uint32 Width() const;
		Uint32 Height() const;

		Uint32 PositionX() const;
		Uint32 PositionY() const;

		bool Loop();
		void Quit(Sint32 exit_code);

		void* Handle() const;
		bool  IsActive() const;

		WindowEvent& GetWindowEvent() { return window_event; }
		void BroadcastEvent(WindowEventData const&);

	private:
		HWND hwnd = nullptr;
		WindowEvent window_event;
	};
}