#pragma once
#include <array>
#include "Windows.h"
#include "Utilities/Delegate.h"
#include "Utilities/Singleton.h"

namespace adria
{
	enum class KeyCode : Uint32
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
		MouseLeft,
		MouseMiddle,
		MouseRight,
		Tilde,
		Count
	};

	class Window;
	struct WindowEventData;

	DECLARE_EVENT(WindowResizedEvent, Input, Uint32, Uint32)
	DECLARE_EVENT(RightMouseClickedEvent, Input, Int32, Int32)
	DECLARE_EVENT(MiddleMouseScrolledEvent, Input, Int32)
	DECLARE_EVENT(F5PressedEvent, Input)
	DECLARE_EVENT(PrintScreenPressedEvent, Input, Char const*)
	struct InputEvents
	{
		MiddleMouseScrolledEvent scroll_mouse_event;
		RightMouseClickedEvent right_mouse_clicked;
		WindowResizedEvent window_resized_event;
		F5PressedEvent f5_pressed_event;
		PrintScreenPressedEvent f6_pressed_event;
	};

	class Input : public Singleton<Input>
	{
		friend class Singleton<Input>;

	public:
		void Initialize(Window* _window)
		{
			window = _window;
		}

		InputEvents& GetInputEvents() { return input_events; }
		void Tick();
		void OnWindowEvent(WindowEventData const&);

		Bool GetKey(KeyCode key)    const { return keys[(Uint64)key]; }
		Bool IsKeyDown(KeyCode key) const { return GetKey(key) && !prev_keys[(Uint64)key]; }
		Bool IsKeyUp(KeyCode key)   const { return !GetKey(key) && prev_keys[(Uint64)key]; }

		void SetMouseVisibility(Bool visible);
		void SetMousePosition(Float xpos, Float ypos);

		Float GetMousePositionX()  const { return mouse_position_x; }
		Float GetMousePositionY()  const { return mouse_position_y; }

		Float GetMouseDeltaX()     const { return mouse_position_x - prev_mouse_position_x; }
		Float GetMouseDeltaY()     const { return mouse_position_y - prev_mouse_position_y; }
		Float GetMouseWheelDelta() const { return mmouse_wheel_delta; }

	private:
		InputEvents input_events;
		std::array<Bool, (Uint64)KeyCode::Count> keys;
		std::array<Bool, (Uint64)KeyCode::Count> prev_keys;

		Float mouse_position_x = 0.0f;
		Float mouse_position_y = 0.0f;

		Float prev_mouse_position_x = 0.0f;
		Float prev_mouse_position_y = 0.0f;
		Float mmouse_wheel_delta = 0.0f;

		Bool new_frame = false;
		Bool resizing = false;

		Window* window = nullptr;

	private:
		Input();
	};
	#define g_Input Input::Get()
}