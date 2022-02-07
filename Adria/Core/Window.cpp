#include "Window.h"
#include "../Utilities/StringUtil.h"
#include "../resource.h"
#include "../Core/Macros.h"

namespace adria
{
    void Window::Initialize(window_init_t const& init)
    {
        _instance = init.instance;
        const std::wstring window_title = ConvertToWide(init.title);
        const uint32 window_width = init.width;
        const uint32 window_height = init.height;
        const LPCWSTR class_name = L"MyWindowClass";

        // Register the Window Class

        WNDCLASSEX wcex{};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = _instance;
        wcex.hIcon = LoadIcon(_instance, MAKEINTRESOURCE(IDI_ICON1));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 2);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = class_name;
        wcex.hIconSm = nullptr;

        if (!::RegisterClassExW(&wcex))
            ::MessageBoxA(nullptr, "Window Class Registration Failed!", "Fatal Error!", MB_ICONEXCLAMATION | MB_OK);
        

        _handle = ::CreateWindowExW
        (
            0, //WS_EX_CLIENTEDGE,
            class_name,
            window_title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height,
            nullptr, nullptr, _instance, nullptr
        );

        if (!_handle)
        {
            MessageBox(nullptr, L"Window Creation Failed!", L"Fatal Error!", MB_ICONEXCLAMATION | MB_OK);
            return;
        }

		::SetWindowLong(_handle, GWL_STYLE, ::GetWindowLong(_handle, GWL_STYLE) & ~WS_MINIMIZEBOX);

        if(init.maximize) ::ShowWindow(_handle, SW_SHOWMAXIMIZED);
        else ::ShowWindow(_handle, SW_SHOWNORMAL);
        ::UpdateWindow(_handle);
        ::SetFocus(_handle);
    }
    uint32 Window::Width()
	{
		RECT rect{};
        ::GetClientRect(_handle, &rect);
		return static_cast<uint32>(rect.right - rect.left);
	}
    uint32 Window::Height()
    {
        RECT rect{};
        ::GetClientRect(_handle, &rect);
        return static_cast<uint32>(rect.bottom - rect.top);
    }
    std::pair<uint32, uint32> Window::Position()
    {
        RECT rect{};
        ::GetClientRect(_handle, &rect);

        ClientToScreen(_handle, (LPPOINT)&rect.left);
        ClientToScreen(_handle, (LPPOINT)&rect.right);

        return { rect.left, rect.top };
    }
    void Window::SetCallback(std::function<void(window_message_t const& window_data)> callback)
    {
        _msg_callback = callback;
    }
	LRESULT Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        LRESULT result = 0ll;

        window_message_t window_data = {};
        window_data.handle = static_cast<void*>(_handle);
        window_data.instance = static_cast<void*>(_instance);
        window_data.msg = static_cast<uint32>(msg);
        window_data.wparam = static_cast<uint64>(wParam);
        window_data.lparam = static_cast<int64>(lParam);
        window_data.width = static_cast<float32>(Width());
        window_data.height = static_cast<float32>(Height());

        if (msg == WM_CLOSE || msg == WM_DESTROY)
        {
            ::PostQuitMessage(0);
            return 0;
        }
        else if (msg == WM_DISPLAYCHANGE || msg == WM_SIZE)
        {
            window_data.width = static_cast<float>(lParam & 0xffff);
            window_data.height = static_cast<float>((lParam >> 16) & 0xffff);
        }
        else result = DefWindowProc(hwnd, msg, wParam, lParam);

        if (_msg_callback)
        {
            _msg_callback(window_data);
        }

        return result;
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

	void Window::Quit(int exit_code)
	{
        ::PostQuitMessage(exit_code);
	}

	void Window::Destroy()
    {
        _msg_callback = nullptr;
        if (_handle) ::DestroyWindow(_handle);
    }
    void* Window::Handle()
    {
        return static_cast<void*>(_handle);
    }
    bool Window::IsActive()
    {
        return ::GetForegroundWindow() == _handle;
    }
}
