#pragma once
#include <limits>
#include <concepts>
#include "../Core/Definitions.h"


namespace adria
{
	template<typename T>
	concept FloatingPoint = std::is_floating_point_v<T>;

	template<FloatingPoint T = f32>
	constexpr T INF = (std::numeric_limits<T>::max)();

	template<FloatingPoint T = f32>
	constexpr T pi = T(3.141592654f);

	template<FloatingPoint T = f32>
	constexpr T pi_div_2 = pi<T> / 2.0f;

	template<FloatingPoint T = f32>
	constexpr T pi_div_4 = pi<T> / 4.0f;

	template<FloatingPoint T = f32>
	constexpr T pi_times_2 = pi<T> * 2.0f;

	template<FloatingPoint T = f32>
	constexpr T pi_times_4 = pi<T> * 4.0f;

	template<FloatingPoint T = f32>
	constexpr T pi_squared = pi<T> * pi;

	template<FloatingPoint T = f32>
	constexpr T pi_div_180 = pi<T> / 180.0f;
}

