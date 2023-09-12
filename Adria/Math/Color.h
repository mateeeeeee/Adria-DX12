#pragma once
#include <DirectXMath.h>

using namespace DirectX;

namespace adria
{
	struct Color
	{
		uint32 rgba = 0;

		constexpr Color(uint32 rgba) : rgba(rgba) {}
		constexpr Color(uint8 r = 0, uint8 g = 0, uint8 b = 0, uint8 a = 255) : rgba(uint32(r) | (uint32(g) << 8) | (uint32(b) << 16) | (uint32(a) << 24)) {}
		
		constexpr uint8 GetR() const { return (rgba >> 0) & 0xFF; }
		constexpr uint8 GetG() const { return (rgba >> 8) & 0xFF; }
		constexpr uint8 GetB() const { return (rgba >> 16) & 0xFF; }
		constexpr uint8 GetA() const { return (rgba >> 24) & 0xFF; }

		constexpr void SetR(uint8_t value) { *this = Color(value, GetG(), GetB(), GetA()); }
		constexpr void SetG(uint8_t value) { *this = Color(GetR(), value, GetB(), GetA()); }
		constexpr void SetB(uint8_t value) { *this = Color(GetR(), GetG(), value, GetA()); }
		constexpr void SetA(uint8_t value) { *this = Color(GetR(), GetG(), GetB(), value); }

		constexpr XMFLOAT3 ToFloat3() const
		{
			return XMFLOAT3(
				((rgba >> 0) & 0xFF) / 255.0f,
				((rgba >> 8) & 0xFF) / 255.0f,
				((rgba >> 16) & 0xFF) / 255.0f
			);
		}
		constexpr XMFLOAT4 toFloat4() const
		{
			return XMFLOAT4(
				((rgba >> 0) & 0xFF) / 255.0f,
				((rgba >> 8) & 0xFF) / 255.0f,
				((rgba >> 16) & 0xFF) / 255.0f,
				((rgba >> 24) & 0xFF) / 255.0f
			);
		}
		constexpr operator XMFLOAT3() const { return ToFloat3(); }
		constexpr operator XMFLOAT4() const { return toFloat4(); }
		constexpr operator uint32_t() const { return rgba; }

		
		static constexpr Color FromFloat4(const XMFLOAT4& value)
		{
			return Color((uint8_t)(value.x * 255), (uint8_t)(value.y * 255), (uint8_t)(value.z * 255), (uint8_t)(value.w * 255));
		}
		static constexpr Color FromFloat3(const XMFLOAT3& value)
		{
			return Color((uint8_t)(value.x * 255), (uint8_t)(value.y * 255), (uint8_t)(value.z * 255));
		}

		static constexpr Color Red() { return Color(255, 0, 0, 255); }
		static constexpr Color Green() { return Color(0, 255, 0, 255); }
		static constexpr Color Blue() { return Color(0, 0, 255, 255); }
		static constexpr Color Black() { return Color(0, 0, 0, 255); }
		static constexpr Color White() { return Color(255, 255, 255, 255); }
		static constexpr Color Yellow() { return Color(255, 255, 0, 255); }
		static constexpr Color Purple() { return Color(255, 0, 255, 255); }
		static constexpr Color Cyan() { return Color(0, 255, 255, 255); }
		static constexpr Color Transparent() { return Color(0, 0, 0, 0); }
		static constexpr Color Gray() { return Color(127, 127, 127, 255); }
		static constexpr Color Ghost() { return Color(127, 127, 127, 127); }
		static constexpr Color Booger() { return Color(127, 127, 127, 200); }
		static constexpr Color Shadow() { return Color(0, 0, 0, 100); }

		static constexpr Color Warning() { return 0xFF66FFFF; } // light yellow
		static constexpr Color Error() { return 0xFF6666FF; }   // light red
	};
}