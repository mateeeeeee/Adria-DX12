#include "Window.h"
#include "resource.h"
#include "Utilities/StringUtil.h"

namespace adria
{
	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
	{
		Window* this_window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		WindowEventData window_data{};
		window_data.handle = hwnd;
		window_data.msg  = static_cast<uint32>(msg);
		window_data.wparam = static_cast<uint64>(w_param);
		window_data.lparam = static_cast<int64>(l_param);
		window_data.width = this_window ? static_cast<float>(this_window->Width()) : 0.0f;
		window_data.height = this_window ? static_cast<float>(this_window->Height()) : 0.0f;

		LRESULT result = 0ll;
		if (msg == WM_CLOSE || msg == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		else if (msg == WM_DISPLAYCHANGE || msg == WM_SIZE)
		{
			window_data.width = static_cast<float>(l_param & 0xffff);
			window_data.height = static_cast<float>((l_param >> 16) & 0xffff);
		}
		else result = DefWindowProc(hwnd, msg, w_param, l_param);
        if (this_window) this_window->BroadcastEvent(window_data);
		return result;
	}


    Window::Window(WindowInit const& init)
    {
        HINSTANCE hinstance = GetModuleHandle(NULL);
        const std::wstring window_title = ToWideString(init.title);
        const uint32  window_width = init.width;
        const uint32  window_height = init.height;
        const LPCWSTR class_name = L"AdriaClass";

        WNDCLASSEX wcex{};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hinstance;
        wcex.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_ICON1));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 2);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = class_name;
        wcex.hIconSm = nullptr;

        if (!RegisterClassExW(&wcex)) MessageBoxA(nullptr, "Window class registration failed!", "Fatal Error!", MB_ICONEXCLAMATION | MB_OK);
        hwnd = CreateWindowExW
        (
            0, class_name,
            window_title.c_str(),
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
            window_width, window_height,
            nullptr, nullptr, hinstance, nullptr
        );

        if (!hwnd)
        {
            MessageBox(nullptr, L"Window creation failed!", L"Fatal Error!", MB_ICONEXCLAMATION | MB_OK);
            return;
        }

		SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        if(init.maximize) ShowWindow(hwnd, SW_SHOWMAXIMIZED);
        else ShowWindow(hwnd, SW_SHOWNORMAL);

		UpdateWindow(hwnd);
		SetFocus(hwnd);
    }

	Window::~Window()
	{
		if (hwnd) DestroyWindow(hwnd);
	}

	uint32 Window::Width() const
	{
		RECT rect{};
        GetClientRect(hwnd, &rect);
		return static_cast<uint32>(rect.right - rect.left);
	}
    uint32 Window::Height() const
    {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        return static_cast<uint32>(rect.bottom - rect.top);
    }
    uint32 Window::PositionX() const
    {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        ClientToScreen(hwnd, (LPPOINT)&rect.left);
        ClientToScreen(hwnd, (LPPOINT)&rect.right);
        return rect.left;
    }
	uint32 Window::PositionY() const
	{
		RECT rect{};
		GetClientRect(hwnd, &rect);
		ClientToScreen(hwnd, (LPPOINT)&rect.left);
		ClientToScreen(hwnd, (LPPOINT)&rect.right);
		return rect.top;
	}
    
	bool Window::Loop()
    {
        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) return false;
        }
        return true;
    }

	void Window::Quit(int32 exit_code)
	{
        PostQuitMessage(exit_code);
	}

    void* Window::Handle() const
    {
        return static_cast<void*>(hwnd);
    }
    bool Window::IsActive() const
    {
        return GetForegroundWindow() == hwnd;
    }

	void Window::BroadcastEvent(WindowEventData const& data)
	{
        window_event.Broadcast(data);
	}

}
