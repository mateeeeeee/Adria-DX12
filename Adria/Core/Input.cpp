#include <windowsx.h>
#include "Input.h"
#include "Window.h"
#include "Logging/Logger.h"

namespace adria
{
	
	inline Bool IsPressed(Int32 key_code)
	{
		return (::GetKeyState(key_code) & 0x8000) != 0;
	}


	Input::Input() : keys{}, prev_keys{}, input_events{}
	{
		POINT mouse_screen_pos;
		if (GetCursorPos(&mouse_screen_pos))
		{
			mouse_position_x = static_cast<Float>(mouse_screen_pos.x);
			mouse_position_y = static_cast<Float>(mouse_screen_pos.y);
		}
	}

	void Input::Tick()
	{
		prev_keys = std::move(keys);

		prev_mouse_position_x = mouse_position_x;
		prev_mouse_position_y = mouse_position_y;

		if (window->IsActive())
		{
			POINT mouse_screen_pos;
			if (GetCursorPos(&mouse_screen_pos))
			{
				mouse_position_x = static_cast<Float>(mouse_screen_pos.x);
				mouse_position_y = static_cast<Float>(mouse_screen_pos.y);
			}

			using enum KeyCode;

			keys[(Uint64)MouseLeft] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
			keys[(Uint64)MouseMiddle] = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;
			keys[(Uint64)MouseRight] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;

			keys[(Uint64)F1] = IsPressed(VK_F1);
			keys[(Uint64)F2] = IsPressed(VK_F2);
			keys[(Uint64)F3] = IsPressed(VK_F3);
			keys[(Uint64)F4] = IsPressed(VK_F4);
			keys[(Uint64)F5] = IsPressed(VK_F5);
			keys[(Uint64)F6] = IsPressed(VK_F6);
			keys[(Uint64)F7] = IsPressed(VK_F7);
			keys[(Uint64)F8] = IsPressed(VK_F8);
			keys[(Uint64)F9] = IsPressed(VK_F9);
			keys[(Uint64)F10] = IsPressed(VK_F10);
			keys[(Uint64)F11] = IsPressed(VK_F11);
			keys[(Uint64)F12] = IsPressed(VK_F12);
			keys[(Uint64)Alpha0] = IsPressed('0');
			keys[(Uint64)Alpha1] = IsPressed('1');
			keys[(Uint64)Alpha2] = IsPressed('2');
			keys[(Uint64)Alpha3] = IsPressed('3');
			keys[(Uint64)Alpha4] = IsPressed('4');
			keys[(Uint64)Alpha5] = IsPressed('5');
			keys[(Uint64)Alpha6] = IsPressed('6');
			keys[(Uint64)Alpha7] = IsPressed('7');
			keys[(Uint64)Alpha8] = IsPressed('8');
			keys[(Uint64)Alpha9] = IsPressed('9');
			keys[(Uint64)Numpad0] = IsPressed(VK_NUMPAD0);
			keys[(Uint64)Numpad1] = IsPressed(VK_NUMPAD1);
			keys[(Uint64)Numpad2] = IsPressed(VK_NUMPAD2);
			keys[(Uint64)Numpad3] = IsPressed(VK_NUMPAD3);
			keys[(Uint64)Numpad4] = IsPressed(VK_NUMPAD4);
			keys[(Uint64)Numpad5] = IsPressed(VK_NUMPAD5);
			keys[(Uint64)Numpad6] = IsPressed(VK_NUMPAD6);
			keys[(Uint64)Numpad7] = IsPressed(VK_NUMPAD7);
			keys[(Uint64)Numpad8] = IsPressed(VK_NUMPAD8);
			keys[(Uint64)Numpad9] = IsPressed(VK_NUMPAD9);
			keys[(Uint64)Q] = IsPressed('Q');
			keys[(Uint64)W] = IsPressed('W');
			keys[(Uint64)E] = IsPressed('E');
			keys[(Uint64)R] = IsPressed('R');
			keys[(Uint64)T] = IsPressed('T');
			keys[(Uint64)Y] = IsPressed('Y');
			keys[(Uint64)U] = IsPressed('U');
			keys[(Uint64)I] = IsPressed('I');
			keys[(Uint64)O] = IsPressed('O');
			keys[(Uint64)P] = IsPressed('P');
			keys[(Uint64)A] = IsPressed('A');
			keys[(Uint64)S] = IsPressed('S');
			keys[(Uint64)D] = IsPressed('D');
			keys[(Uint64)F] = IsPressed('F');
			keys[(Uint64)G] = IsPressed('G');
			keys[(Uint64)H] = IsPressed('H');
			keys[(Uint64)J] = IsPressed('J');
			keys[(Uint64)K] = IsPressed('K');
			keys[(Uint64)L] = IsPressed('L');
			keys[(Uint64)Z] = IsPressed('Z');
			keys[(Uint64)X] = IsPressed('X');
			keys[(Uint64)C] = IsPressed('C');
			keys[(Uint64)V] = IsPressed('V');
			keys[(Uint64)B] = IsPressed('B');
			keys[(Uint64)N] = IsPressed('N');
			keys[(Uint64)M] = IsPressed('M');
			keys[(Uint64)Esc] = IsPressed(VK_ESCAPE);
			keys[(Uint64)Tab] = IsPressed(VK_TAB);
			keys[(Uint64)ShiftLeft] = IsPressed(VK_LSHIFT);
			keys[(Uint64)ShiftRight] = IsPressed(VK_RSHIFT);
			keys[(Uint64)CtrlLeft] = IsPressed(VK_LCONTROL);
			keys[(Uint64)CtrlRight] = IsPressed(VK_RCONTROL);
			keys[(Uint64)AltLeft] = IsPressed(VK_LMENU);
			keys[(Uint64)AltRight] = IsPressed(VK_RMENU);
			keys[(Uint64)Space] = IsPressed(VK_SPACE);
			keys[(Uint64)CapsLock] = IsPressed(VK_CAPITAL);
			keys[(Uint64)Backspace] = IsPressed(VK_BACK);
			keys[(Uint64)Enter] = IsPressed(VK_RETURN);
			keys[(Uint64)Delete] = IsPressed(VK_DELETE);
			keys[(Uint64)ArrowLeft] = IsPressed(VK_LEFT);
			keys[(Uint64)ArrowRight] = IsPressed(VK_RIGHT);
			keys[(Uint64)ArrowUp] = IsPressed(VK_UP);
			keys[(Uint64)ArrowDown] = IsPressed(VK_DOWN);
			keys[(Uint64)PageUp] = IsPressed(VK_PRIOR);
			keys[(Uint64)PageDown] = IsPressed(VK_NEXT);
			keys[(Uint64)Home] = IsPressed(VK_HOME);
			keys[(Uint64)End] = IsPressed(VK_END);
			keys[(Uint64)Insert] = IsPressed(VK_INSERT);
			keys[(Uint64)Tilde] = IsPressed(VK_OEM_3);

			if (GetKey(KeyCode::Esc)) PostQuitMessage(0);
		}

	}
	void Input::OnWindowEvent(WindowEventInfo const& data)
	{
		HWND handle = static_cast<HWND>(data.handle);
		{
			switch (data.msg)
			{
			case WM_ENTERSIZEMOVE:
				resizing = true;
				break;
			case WM_EXITSIZEMOVE:
				resizing = false;
				input_events.window_resized_event.Broadcast((Uint32)data.width, (Uint32)data.height);
				break;
			case WM_SIZE:
				if (!resizing) input_events.window_resized_event.Broadcast((Uint32)data.width, (Uint32)data.height);
				break;
			case WM_MOUSEWHEEL:
				input_events.scroll_mouse_event.Broadcast((Int32)GET_WHEEL_DELTA_WPARAM(data.wparam) / WHEEL_DELTA);
				break;
			case WM_RBUTTONDOWN:
			{
				Int32 mx = GET_X_LPARAM(data.lparam);
				Int32 my = GET_Y_LPARAM(data.lparam);
				input_events.right_mouse_clicked.Broadcast(mx, my);
			}
			break;
			case WM_KEYDOWN:
				if (data.wparam == VK_F5)
				{
					input_events.f5_pressed_event.Broadcast();
				}
				else if (data.wparam == VK_F6)
				{
					input_events.f6_pressed_event.Broadcast("");
				}
				break;
			}
		}
		mmouse_wheel_delta = (Float)GET_WHEEL_DELTA_WPARAM(data.wparam) / (Float)WHEEL_DELTA;

	}
	void Input::SetMouseVisibility(Bool visible)
	{
		::ShowCursor(visible);
	}
	void Input::SetMousePosition(Float xpos, Float ypos)
	{
		HWND handle = static_cast<HWND>(window->Handle());
		if (handle == ::GetActiveWindow())
		{
			POINT mouse_screen_pos = POINT{ static_cast<LONG>(xpos), static_cast<LONG>(ypos) };
			if (::ClientToScreen(handle, &mouse_screen_pos))  ::SetCursorPos(mouse_screen_pos.x, mouse_screen_pos.y);
		}
	}


}