#include <windowsx.h>
#include "Input.h"
#include "../Core/Window.h"
#include "../Logging/Logger.h"

namespace adria
{

	inline bool IsPressed(int32 key_code)
	{
		return (::GetKeyState(key_code) & 0x8000) != 0;
	}


	Input::Input() : keys{}, prev_keys{}, input_events{}
	{
		HWND handle = static_cast<HWND>(Window::Handle());
		if (!handle)
		{
			ADRIA_LOG(ERROR, "Window handle is NULL! Have you made a call to Window::Initialize?");
			return;
		}
		POINT mouse_screen_pos;
		if (::GetCursorPos(&mouse_screen_pos))
		{
			mouse_position_x = static_cast<float32>(mouse_screen_pos.x);
			mouse_position_y = static_cast<float32>(mouse_screen_pos.y);
		}
	}

	void Input::NewFrame()
	{
		prev_keys = std::move(keys);

		prev_mouse_position_x = mouse_position_x;
		prev_mouse_position_y = mouse_position_y;

		if (Window::IsActive())
		{
			POINT mouse_screen_pos;
			if (::GetCursorPos(&mouse_screen_pos))
			{
				mouse_position_x = static_cast<float32>(mouse_screen_pos.x);
				mouse_position_y = static_cast<float32>(mouse_screen_pos.y);
			}

			//mouse 
			{
				// Keys
				keys[EKeyCode::MouseLeft] = (::GetKeyState(VK_LBUTTON) & 0x8000) != 0; // Left button pressed
				keys[EKeyCode::MouseMiddle] = (::GetKeyState(VK_MBUTTON) & 0x8000) != 0; // Middle button pressed
				keys[EKeyCode::MouseRight] = (::GetKeyState(VK_RBUTTON) & 0x8000) != 0; // Right button pressed
			}

			//keyboard
			{

				keys[EKeyCode::F1] = IsPressed(VK_F1);
				keys[EKeyCode::F2] = IsPressed(VK_F2);
				keys[EKeyCode::F3] = IsPressed(VK_F3);
				keys[EKeyCode::F4] = IsPressed(VK_F4);
				keys[EKeyCode::F5] = IsPressed(VK_F5);
				keys[EKeyCode::F6] = IsPressed(VK_F6);
				keys[EKeyCode::F7] = IsPressed(VK_F7);
				keys[EKeyCode::F8] = IsPressed(VK_F8);
				keys[EKeyCode::F9] = IsPressed(VK_F9);
				keys[EKeyCode::F10] = IsPressed(VK_F10);
				keys[EKeyCode::F11] = IsPressed(VK_F11);
				keys[EKeyCode::F12] = IsPressed(VK_F12);
				keys[EKeyCode::Alpha0] = IsPressed('0');
				keys[EKeyCode::Alpha1] = IsPressed('1');
				keys[EKeyCode::Alpha2] = IsPressed('2');
				keys[EKeyCode::Alpha3] = IsPressed('3');
				keys[EKeyCode::Alpha4] = IsPressed('4');
				keys[EKeyCode::Alpha5] = IsPressed('5');
				keys[EKeyCode::Alpha6] = IsPressed('6');
				keys[EKeyCode::Alpha7] = IsPressed('7');
				keys[EKeyCode::Alpha8] = IsPressed('8');
				keys[EKeyCode::Alpha9] = IsPressed('9');
				keys[EKeyCode::Numpad0] = IsPressed(VK_NUMPAD0);
				keys[EKeyCode::Numpad1] = IsPressed(VK_NUMPAD1);
				keys[EKeyCode::Numpad2] = IsPressed(VK_NUMPAD2);
				keys[EKeyCode::Numpad3] = IsPressed(VK_NUMPAD3);
				keys[EKeyCode::Numpad4] = IsPressed(VK_NUMPAD4);
				keys[EKeyCode::Numpad5] = IsPressed(VK_NUMPAD5);
				keys[EKeyCode::Numpad6] = IsPressed(VK_NUMPAD6);
				keys[EKeyCode::Numpad7] = IsPressed(VK_NUMPAD7);
				keys[EKeyCode::Numpad8] = IsPressed(VK_NUMPAD8);
				keys[EKeyCode::Numpad9] = IsPressed(VK_NUMPAD9);
				keys[EKeyCode::Q] = IsPressed('Q');
				keys[EKeyCode::W] = IsPressed('W');
				keys[EKeyCode::E] = IsPressed('E');
				keys[EKeyCode::R] = IsPressed('R');
				keys[EKeyCode::T] = IsPressed('T');
				keys[EKeyCode::Y] = IsPressed('Y');
				keys[EKeyCode::U] = IsPressed('U');
				keys[EKeyCode::I] = IsPressed('I');
				keys[EKeyCode::O] = IsPressed('O');
				keys[EKeyCode::P] = IsPressed('P');
				keys[EKeyCode::A] = IsPressed('A');
				keys[EKeyCode::S] = IsPressed('S');
				keys[EKeyCode::D] = IsPressed('D');
				keys[EKeyCode::F] = IsPressed('F');
				keys[EKeyCode::G] = IsPressed('G');
				keys[EKeyCode::H] = IsPressed('H');
				keys[EKeyCode::J] = IsPressed('J');
				keys[EKeyCode::K] = IsPressed('K');
				keys[EKeyCode::L] = IsPressed('L');
				keys[EKeyCode::Z] = IsPressed('Z');
				keys[EKeyCode::X] = IsPressed('X');
				keys[EKeyCode::C] = IsPressed('C');
				keys[EKeyCode::V] = IsPressed('V');
				keys[EKeyCode::B] = IsPressed('B');
				keys[EKeyCode::N] = IsPressed('N');
				keys[EKeyCode::M] = IsPressed('M');
				keys[EKeyCode::Esc] = IsPressed(VK_ESCAPE);
				keys[EKeyCode::Tab] = IsPressed(VK_TAB);
				keys[EKeyCode::ShiftLeft] = IsPressed(VK_LSHIFT);
				keys[EKeyCode::ShiftRight] = IsPressed(VK_RSHIFT);
				keys[EKeyCode::CtrlLeft] = IsPressed(VK_LCONTROL);
				keys[EKeyCode::CtrlRight] = IsPressed(VK_RCONTROL);
				keys[EKeyCode::AltLeft] = IsPressed(VK_LMENU);
				keys[EKeyCode::AltRight] = IsPressed(VK_RMENU);
				keys[EKeyCode::Space] = IsPressed(VK_SPACE);
				keys[EKeyCode::CapsLock] = IsPressed(VK_CAPITAL);
				keys[EKeyCode::Backspace] = IsPressed(VK_BACK);
				keys[EKeyCode::Enter] = IsPressed(VK_RETURN);
				keys[EKeyCode::Delete] = IsPressed(VK_DELETE);
				keys[EKeyCode::ArrowLeft] = IsPressed(VK_LEFT);
				keys[EKeyCode::ArrowRight] = IsPressed(VK_RIGHT);
				keys[EKeyCode::ArrowUp] = IsPressed(VK_UP);
				keys[EKeyCode::ArrowDown] = IsPressed(VK_DOWN);
				keys[EKeyCode::PageUp] = IsPressed(VK_PRIOR);
				keys[EKeyCode::PageDown] = IsPressed(VK_NEXT);
				keys[EKeyCode::Home] = IsPressed(VK_HOME);
				keys[EKeyCode::End] = IsPressed(VK_END);
				keys[EKeyCode::Insert] = IsPressed(VK_INSERT);
			}
			if (GetKey(EKeyCode::Esc)) PostQuitMessage(0);
		}

	}
	void Input::HandleWindowMessage(WindowMessage const& data)
	{
		HWND handle = static_cast<HWND>(Window::Handle());
		//events
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
			}
		}

		m_mouse_wheel_delta = (float32)GET_WHEEL_DELTA_WPARAM(data.wparam) / (float32)WHEEL_DELTA;

	}
	void Input::SetMouseVisible(bool visible)
	{
		::ShowCursor(visible);
	}
	void Input::SetMousePosition(float32 xpos, float32 ypos)
	{
		HWND handle = static_cast<HWND>(Window::Handle());
		if (handle == ::GetActiveWindow())
		{
			POINT mouse_screen_pos = POINT{ static_cast<LONG>(xpos), static_cast<LONG>(ypos) };
			if (::ClientToScreen(handle, &mouse_screen_pos))  ::SetCursorPos(mouse_screen_pos.x, mouse_screen_pos.y);
		}
	}


}