#pragma once
#include <limits>
#include <concepts>

namespace adria
{
	template<typename T>
	concept FloatingPoint = std::is_floating_point_v<T>;

	template<FloatingPoint T = float>
	constexpr T INF = (std::numeric_limits<T>::max)();

	template<FloatingPoint T = float>
	constexpr T EPSILON = std::numeric_limits<T>::epsilon();

	template<FloatingPoint T = float>
	constexpr T pi = T(3.141592654f);

	template<FloatingPoint T = float>
	constexpr T pi_div_2 = pi<T> / 2.0f;

	template<FloatingPoint T = float>
	constexpr T pi_div_4 = pi<T> / 4.0f;

	template<FloatingPoint T = float>
	constexpr T pi_times_2 = pi<T> * 2.0f;

	template<FloatingPoint T = float>
	constexpr T pi_times_4 = pi<T> * 4.0f;

	template<FloatingPoint T = float>
	constexpr T pi_squared = pi<T> * pi;

	template<FloatingPoint T = float>
	constexpr T pi_div_180 = pi<T> / 180.0f;
}

