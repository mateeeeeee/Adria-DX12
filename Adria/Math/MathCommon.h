#pragma once
#include "MathTypes.h"

namespace adria
{
	template<typename T>
	inline constexpr T DivideAndRoundDown(T nominator, T denominator)
	{
		return nominator / denominator;
	}
	template<typename T>
	inline constexpr T DivideAndRoundUp(T nominator, T denominator)
	{
		return (nominator + denominator - 1) / denominator;
	}

	template<typename T>
	inline constexpr T Min(T a)
	{
		return a;
	}

	template<typename T, typename... Args>
	inline constexpr T Min(T a, Args... args)
	{
		T _min = Min(args...);
		return a < _min ? a : _min;
	}

	template<typename T>
	inline constexpr T Max(T a)
	{
		return a;
	}

	template<typename T, typename... Args>
	inline constexpr T Max(T a, Args... args)
	{
		T _max = Max(args...);
		return a > _max ? a : _max;
	}

	template<typename T, typename U, typename V>
	inline constexpr T Clamp(T value, U min, V max)
	{
		return value > max ? (T)max : (value < min ? (T)min : value);
	}

}