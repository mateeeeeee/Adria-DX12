#pragma once
#include "../Core/Definitions.h"
#include "../Core/Windows.h"
#include <array>
#include <unordered_map>

namespace adria
{
    enum class KeyCode : uint32
    {
       
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        Alpha0, Alpha1, Alpha2, Alpha3, Alpha4, Alpha5, Alpha6, Alpha7, Alpha8, Alpha9,
        Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
        Q, W, E, R, T, Y, U, I, O, P,
        A, S, D, F, G, H, J, K, L,
        Z, X, C, V, B, N, M,
        Esc,
        Tab,
        ShiftLeft, ShiftRight,
        CtrlLeft, CtrlRight,
        AltLeft, AltRight,
        Space,
        CapsLock,
        Backspace,
        Enter,
        Delete,
        ArrowLeft, ArrowRight, ArrowUp, ArrowDown,
        PageUp, PageDown,
        Home,
        End,
        Insert,
        ClickLeft,
        ClickMiddle,
        ClickRight
    };

    struct window_message_t;
    class EventQueue;

    class Input
    {
    public:
        Input(EventQueue& event_queue);

        void NewFrame();

        void HandleWindowMessage(window_message_t const&);

        bool GetKey(KeyCode key)    /*const*/   { return keys[key]; }                         
        bool IsKeyDown(KeyCode key) /*const*/   { return GetKey(key) && !prev_keys[key]; }
        bool IsKeyUp(KeyCode key)   /*const*/   { return !GetKey(key) && prev_keys[key]; }

        // Mouse
        void SetMouseVisible(bool visible);
        void SetMousePosition(float32 xpos, float32 ypos);

        float32 GetMousePositionX()  const { return mouse_position_x; }
        float32 GetMousePositionY()  const { return mouse_position_y; }

        float32 GetMouseDeltaX()     const { return mouse_position_x - prev_mouse_position_x;/*return mouse_delta_x;*/ }
        float32 GetMouseDeltaY()     const { return mouse_position_y - prev_mouse_position_y;/*return mouse_delta_y;*/ }
        float32 GetMouseWheelDelta() const { return m_mouse_wheel_delta; }


    private:
        EventQueue& event_queue;

        std::unordered_map<KeyCode, bool> keys;
        std::unordered_map<KeyCode, bool> prev_keys;
        // Mouse
        float32 mouse_position_x = 0.0f;
        float32 mouse_position_y = 0.0f;

        float32 prev_mouse_position_x = 0.0f;
        float32 prev_mouse_position_y = 0.0f;
        float32 m_mouse_wheel_delta = 0.0f;

        bool new_frame = false;
        bool resizing = false;
    };
}