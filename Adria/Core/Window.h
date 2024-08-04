#pragma once
#include "Windows.h"
#include "Utilities/Delegate.h"

namespace adria
{
	struct WindowEventData
	{
		void*  handle	= nullptr;
		uint32 msg		= 0;
        uint64 wparam	= 0;
        int64  lparam	= 0;
		float  width	= 0.0f;
		float  height	= 0.0f;
	};

    struct WindowInit
    {
        char const* title;
        uint32 width, height;
        bool maximize;
    };

	DECLARE_EVENT(WindowEvent, Window, WindowEventData const&)

	class Window
	{
	public:
		Window(WindowInit const& init);
		~Window();

		uint32 Width() const;
		uint32 Height() const;

		uint32 PositionX() const;
		uint32 PositionY() const;

		bool Loop();
		void Quit(int32 exit_code);

		void* Handle() const;
		bool  IsActive() const;

		WindowEvent& GetWindowEvent() { return window_event; }
		void BroadcastEvent(WindowEventData const&);

	private:
		HWND hwnd = nullptr;
		WindowEvent window_event;
	};
}