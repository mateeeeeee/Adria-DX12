#include <windowsx.h>
#include "Input.h"
#include "Window.h"
#include "Logging/Logger.h"

namespace adria
{
	
	inline bool IsPressed(int32 key_code)
	{
		return (::GetKeyState(key_code) & 0x8000) != 0;
	}


	Input::Input() : keys{}, prev_keys{}, input_events{}
	{
		POINT mouse_screen_pos;
		if (GetCursorPos(&mouse_screen_pos))
		{
			mouse_position_x = static_cast<float>(mouse_screen_pos.x);
			mouse_position_y = static_cast<float>(mouse_screen_pos.y);
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
				mouse_position_x = static_cast<float>(mouse_screen_pos.x);
				mouse_position_y = static_cast<float>(mouse_screen_pos.y);
			}

			using enum KeyCode;

			keys[(uint64)MouseLeft] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
			keys[(uint64)MouseMiddle] = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;
			keys[(uint64)MouseRight] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;

			keys[(uint64)F1] = IsPressed(VK_F1);
			keys[(uint64)F2] = IsPressed(VK_F2);
			keys[(uint64)F3] = IsPressed(VK_F3);
			keys[(uint64)F4] = IsPressed(VK_F4);
			keys[(uint64)F5] = IsPressed(VK_F5);
			keys[(uint64)F6] = IsPressed(VK_F6);
			keys[(uint64)F7] = IsPressed(VK_F7);
			keys[(uint64)F8] = IsPressed(VK_F8);
			keys[(uint64)F9] = IsPressed(VK_F9);
			keys[(uint64)F10] = IsPressed(VK_F10);
			keys[(uint64)F11] = IsPressed(VK_F11);
			keys[(uint64)F12] = IsPressed(VK_F12);
			keys[(uint64)Alpha0] = IsPressed('0');
			keys[(uint64)Alpha1] = IsPressed('1');
			keys[(uint64)Alpha2] = IsPressed('2');
			keys[(uint64)Alpha3] = IsPressed('3');
			keys[(uint64)Alpha4] = IsPressed('4');
			keys[(uint64)Alpha5] = IsPressed('5');
			keys[(uint64)Alpha6] = IsPressed('6');
			keys[(uint64)Alpha7] = IsPressed('7');
			keys[(uint64)Alpha8] = IsPressed('8');
			keys[(uint64)Alpha9] = IsPressed('9');
			keys[(uint64)Numpad0] = IsPressed(VK_NUMPAD0);
			keys[(uint64)Numpad1] = IsPressed(VK_NUMPAD1);
			keys[(uint64)Numpad2] = IsPressed(VK_NUMPAD2);
			keys[(uint64)Numpad3] = IsPressed(VK_NUMPAD3);
			keys[(uint64)Numpad4] = IsPressed(VK_NUMPAD4);
			keys[(uint64)Numpad5] = IsPressed(VK_NUMPAD5);
			keys[(uint64)Numpad6] = IsPressed(VK_NUMPAD6);
			keys[(uint64)Numpad7] = IsPressed(VK_NUMPAD7);
			keys[(uint64)Numpad8] = IsPressed(VK_NUMPAD8);
			keys[(uint64)Numpad9] = IsPressed(VK_NUMPAD9);
			keys[(uint64)Q] = IsPressed('Q');
			keys[(uint64)W] = IsPressed('W');
			keys[(uint64)E] = IsPressed('E');
			keys[(uint64)R] = IsPressed('R');
			keys[(uint64)T] = IsPressed('T');
			keys[(uint64)Y] = IsPressed('Y');
			keys[(uint64)U] = IsPressed('U');
			keys[(uint64)I] = IsPressed('I');
			keys[(uint64)O] = IsPressed('O');
			keys[(uint64)P] = IsPressed('P');
			keys[(uint64)A] = IsPressed('A');
			keys[(uint64)S] = IsPressed('S');
			keys[(uint64)D] = IsPressed('D');
			keys[(uint64)F] = IsPressed('F');
			keys[(uint64)G] = IsPressed('G');
			keys[(uint64)H] = IsPressed('H');
			keys[(uint64)J] = IsPressed('J');
			keys[(uint64)K] = IsPressed('K');
			keys[(uint64)L] = IsPressed('L');
			keys[(uint64)Z] = IsPressed('Z');
			keys[(uint64)X] = IsPressed('X');
			keys[(uint64)C] = IsPressed('C');
			keys[(uint64)V] = IsPressed('V');
			keys[(uint64)B] = IsPressed('B');
			keys[(uint64)N] = IsPressed('N');
			keys[(uint64)M] = IsPressed('M');
			keys[(uint64)Esc] = IsPressed(VK_ESCAPE);
			keys[(uint64)Tab] = IsPressed(VK_TAB);
			keys[(uint64)ShiftLeft] = IsPressed(VK_LSHIFT);
			keys[(uint64)ShiftRight] = IsPressed(VK_RSHIFT);
			keys[(uint64)CtrlLeft] = IsPressed(VK_LCONTROL);
			keys[(uint64)CtrlRight] = IsPressed(VK_RCONTROL);
			keys[(uint64)AltLeft] = IsPressed(VK_LMENU);
			keys[(uint64)AltRight] = IsPressed(VK_RMENU);
			keys[(uint64)Space] = IsPressed(VK_SPACE);
			keys[(uint64)CapsLock] = IsPressed(VK_CAPITAL);
			keys[(uint64)Backspace] = IsPressed(VK_BACK);
			keys[(uint64)Enter] = IsPressed(VK_RETURN);
			keys[(uint64)Delete] = IsPressed(VK_DELETE);
			keys[(uint64)ArrowLeft] = IsPressed(VK_LEFT);
			keys[(uint64)ArrowRight] = IsPressed(VK_RIGHT);
			keys[(uint64)ArrowUp] = IsPressed(VK_UP);
			keys[(uint64)ArrowDown] = IsPressed(VK_DOWN);
			keys[(uint64)PageUp] = IsPressed(VK_PRIOR);
			keys[(uint64)PageDown] = IsPressed(VK_NEXT);
			keys[(uint64)Home] = IsPressed(VK_HOME);
			keys[(uint64)End] = IsPressed(VK_END);
			keys[(uint64)Insert] = IsPressed(VK_INSERT);
			keys[(uint64)Tilde] = IsPressed(VK_OEM_3);

			if (GetKey(KeyCode::Esc)) PostQuitMessage(0);
		}

	}
	void Input::OnWindowEvent(WindowEventData const& data)
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
				input_events.window_resized_event.Broadcast((uint32)data.width, (uint32)data.height);
				break;
			case WM_SIZE:
				if (!resizing) input_events.window_resized_event.Broadcast((uint32)data.width, (uint32)data.height);
				break;
			case WM_MOUSEWHEEL:
				input_events.scroll_mouse_event.Broadcast((int32)GET_WHEEL_DELTA_WPARAM(data.wparam) / WHEEL_DELTA);
				break;
			case WM_RBUTTONDOWN:
			{
				int32 mx = GET_X_LPARAM(data.lparam);
				int32 my = GET_Y_LPARAM(data.lparam);
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
		mmouse_wheel_delta = (float)GET_WHEEL_DELTA_WPARAM(data.wparam) / (float)WHEEL_DELTA;

	}
	void Input::SetMouseVisibility(bool visible)
	{
		::ShowCursor(visible);
	}
	void Input::SetMousePosition(float xpos, float ypos)
	{
		HWND handle = static_cast<HWND>(window->Handle());
		if (handle == ::GetActiveWindow())
		{
			POINT mouse_screen_pos = POINT{ static_cast<LONG>(xpos), static_cast<LONG>(ypos) };
			if (::ClientToScreen(handle, &mouse_screen_pos))  ::SetCursorPos(mouse_screen_pos.x, mouse_screen_pos.y);
		}
	}


}