#pragma once
#include <array>
#include "Windows.h"
#include "Utilities/Delegate.h"
#include "Utilities/Singleton.h"

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
		MouseLeft,
		MouseMiddle,
		MouseRight,
		Tilde,
		Count
	};

	class Window;
	struct WindowEventData;

	DECLARE_EVENT(WindowResizedEvent, Input, uint32, uint32)
	DECLARE_EVENT(RightMouseClickedEvent, Input, int32, int32)
	DECLARE_EVENT(MiddleMouseScrolledEvent, Input, int32)
	DECLARE_EVENT(F5PressedEvent, Input)
	DECLARE_EVENT(PrintScreenPressedEvent, Input, char const*)
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

		bool GetKey(KeyCode key)    const { return keys[(uint64)key]; }
		bool IsKeyDown(KeyCode key) const { return GetKey(key) && !prev_keys[(uint64)key]; }
		bool IsKeyUp(KeyCode key)   const { return !GetKey(key) && prev_keys[(uint64)key]; }

		void SetMouseVisibility(bool visible);
		void SetMousePosition(float xpos, float ypos);

		float GetMousePositionX()  const { return mouse_position_x; }
		float GetMousePositionY()  const { return mouse_position_y; }

		float GetMouseDeltaX()     const { return mouse_position_x - prev_mouse_position_x; }
		float GetMouseDeltaY()     const { return mouse_position_y - prev_mouse_position_y; }
		float GetMouseWheelDelta() const { return mmouse_wheel_delta; }

	private:
		InputEvents input_events;
		std::array<bool, (uint64)KeyCode::Count> keys;
		std::array<bool, (uint64)KeyCode::Count> prev_keys;

		float mouse_position_x = 0.0f;
		float mouse_position_y = 0.0f;

		float prev_mouse_position_x = 0.0f;
		float prev_mouse_position_y = 0.0f;
		float mmouse_wheel_delta = 0.0f;

		bool new_frame = false;
		bool resizing = false;

		Window* window = nullptr;

	private:
		Input();
	};
	#define g_Input Input::Get()
}