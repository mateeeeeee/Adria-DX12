#include "Input.h"
#include "../Core/Window.h"
#include "../Logging/Logger.h"
#include "../Events/EventQueue.h"
#include "../Core/Events.h"

namespace adria
{

    inline bool IsPressed(int32 key_code)
    {
        return (::GetKeyState(key_code) & 0x8000) != 0;
    }


    Input::Input(EventQueue& event_queue) : keys{}, prev_keys{}, event_queue{ event_queue }
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
                keys[KeyCode::ClickLeft] = (::GetKeyState(VK_LBUTTON) & 0x8000) != 0; // Left button pressed
                keys[KeyCode::ClickMiddle] = (::GetKeyState(VK_MBUTTON) & 0x8000) != 0; // Middle button pressed
                keys[KeyCode::ClickRight] = (::GetKeyState(VK_RBUTTON) & 0x8000) != 0; // Right button pressed
            }

            //keyboard
            {
                //Use GetKeyboardState()?

                keys[KeyCode::F1] = IsPressed(VK_F1);
                keys[KeyCode::F2] = IsPressed(VK_F2);
                keys[KeyCode::F3] = IsPressed(VK_F3);
                keys[KeyCode::F4] = IsPressed(VK_F4);
                keys[KeyCode::F5] = IsPressed(VK_F5);
                keys[KeyCode::F6] = IsPressed(VK_F6);
                keys[KeyCode::F7] = IsPressed(VK_F7);
                keys[KeyCode::F8] = IsPressed(VK_F8);
                keys[KeyCode::F9] = IsPressed(VK_F9);
                keys[KeyCode::F10] = IsPressed(VK_F10);
                keys[KeyCode::F11] = IsPressed(VK_F11);
                keys[KeyCode::F12] = IsPressed(VK_F12);
                keys[KeyCode::Alpha0] = IsPressed('0');
                keys[KeyCode::Alpha1] = IsPressed('1');
                keys[KeyCode::Alpha2] = IsPressed('2');
                keys[KeyCode::Alpha3] = IsPressed('3');
                keys[KeyCode::Alpha4] = IsPressed('4');
                keys[KeyCode::Alpha5] = IsPressed('5');
                keys[KeyCode::Alpha6] = IsPressed('6');
                keys[KeyCode::Alpha7] = IsPressed('7');
                keys[KeyCode::Alpha8] = IsPressed('8');
                keys[KeyCode::Alpha9] = IsPressed('9');
                keys[KeyCode::Numpad0] = IsPressed(VK_NUMPAD0);
                keys[KeyCode::Numpad1] = IsPressed(VK_NUMPAD1);
                keys[KeyCode::Numpad2] = IsPressed(VK_NUMPAD2);
                keys[KeyCode::Numpad3] = IsPressed(VK_NUMPAD3);
                keys[KeyCode::Numpad4] = IsPressed(VK_NUMPAD4);
                keys[KeyCode::Numpad5] = IsPressed(VK_NUMPAD5);
                keys[KeyCode::Numpad6] = IsPressed(VK_NUMPAD6);
                keys[KeyCode::Numpad7] = IsPressed(VK_NUMPAD7);
                keys[KeyCode::Numpad8] = IsPressed(VK_NUMPAD8);
                keys[KeyCode::Numpad9] = IsPressed(VK_NUMPAD9);
                keys[KeyCode::Q] = IsPressed('Q');
                keys[KeyCode::W] = IsPressed('W');
                keys[KeyCode::E] = IsPressed('E');
                keys[KeyCode::R] = IsPressed('R');
                keys[KeyCode::T] = IsPressed('T');
                keys[KeyCode::Y] = IsPressed('Y');
                keys[KeyCode::U] = IsPressed('U');
                keys[KeyCode::I] = IsPressed('I');
                keys[KeyCode::O] = IsPressed('O');
                keys[KeyCode::P] = IsPressed('P');
                keys[KeyCode::A] = IsPressed('A');
                keys[KeyCode::S] = IsPressed('S');
                keys[KeyCode::D] = IsPressed('D');
                keys[KeyCode::F] = IsPressed('F');
                keys[KeyCode::G] = IsPressed('G');
                keys[KeyCode::H] = IsPressed('H');
                keys[KeyCode::J] = IsPressed('J');
                keys[KeyCode::K] = IsPressed('K');
                keys[KeyCode::L] = IsPressed('L');
                keys[KeyCode::Z] = IsPressed('Z');
                keys[KeyCode::X] = IsPressed('X');
                keys[KeyCode::C] = IsPressed('C');
                keys[KeyCode::V] = IsPressed('V');
                keys[KeyCode::B] = IsPressed('B');
                keys[KeyCode::N] = IsPressed('N');
                keys[KeyCode::M] = IsPressed('M');
                keys[KeyCode::Esc] = IsPressed(VK_ESCAPE);
                keys[KeyCode::Tab] = IsPressed(VK_TAB);
                keys[KeyCode::ShiftLeft] = IsPressed(VK_LSHIFT);
                keys[KeyCode::ShiftRight] = IsPressed(VK_RSHIFT);
                keys[KeyCode::CtrlLeft] = IsPressed(VK_LCONTROL);
                keys[KeyCode::CtrlRight] = IsPressed(VK_RCONTROL);
                keys[KeyCode::AltLeft] = IsPressed(VK_LMENU);
                keys[KeyCode::AltRight] = IsPressed(VK_RMENU);
                keys[KeyCode::Space] = IsPressed(VK_SPACE);
                keys[KeyCode::CapsLock] = IsPressed(VK_CAPITAL);
                keys[KeyCode::Backspace] = IsPressed(VK_BACK);
                keys[KeyCode::Enter] = IsPressed(VK_RETURN);
                keys[KeyCode::Delete] = IsPressed(VK_DELETE);
                keys[KeyCode::ArrowLeft] = IsPressed(VK_LEFT);
                keys[KeyCode::ArrowRight] = IsPressed(VK_RIGHT);
                keys[KeyCode::ArrowUp] = IsPressed(VK_UP);
                keys[KeyCode::ArrowDown] = IsPressed(VK_DOWN);
                keys[KeyCode::PageUp] = IsPressed(VK_PRIOR);
                keys[KeyCode::PageDown] = IsPressed(VK_NEXT);
                keys[KeyCode::Home] = IsPressed(VK_HOME);
                keys[KeyCode::End] = IsPressed(VK_END);
                keys[KeyCode::Insert] = IsPressed(VK_INSERT);
            }
            if (GetKey(KeyCode::Esc)) PostQuitMessage(0);
        }

    }
    void Input::HandleWindowMessage(window_message_t const& data)
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
                event_queue.ConstructAndPushEvent<ResizeEvent>((uint32)data.width, (uint32)data.height);
                break;
            case WM_SIZE:
                if (!resizing) event_queue.ConstructAndPushEvent<ResizeEvent>((uint32)data.width, (uint32)data.height);
                break;
            case WM_MOUSEWHEEL:
                event_queue.ConstructAndPushEvent<ScrollEvent>((int32)GET_WHEEL_DELTA_WPARAM(data.wparam) / WHEEL_DELTA);
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