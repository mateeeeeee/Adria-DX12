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

	inline Vector4 ConvertElevationAndAzimuthToDirection(Float elevation, Float azimuth)
	{
		Float phi = DirectX::XMConvertToRadians(azimuth);
		Float theta = DirectX::XMConvertToRadians(elevation);
		Float x = cos(theta) * sin(phi);
		Float y = sin(theta);
		Float z = cos(theta) * cos(phi);
		return Vector4(x,y,z, 0.0f);
	}

	inline void ConvertDirectionToAzimuthAndElevation(Vector4 const& direction, Float& elevation, Float& azimuth)
	{
		azimuth = atan2(direction.x, direction.z);
		if (azimuth < 0)  azimuth += DirectX::XM_2PI;
		elevation = asin(direction.y);

		azimuth = DirectX::XMConvertToDegrees(azimuth);
		elevation = DirectX::XMConvertToDegrees(elevation);
	}

	inline Color ConvertTemperatureToColor(Float temperature)
	{
		static constexpr Float MIN_TEMPERATURE = 1000.0f;
		static constexpr Float MAX_TEMPERATURE = 15000.0f;
		temperature = Clamp(temperature, MIN_TEMPERATURE, MAX_TEMPERATURE);

		//[Krystek85] Algorithm works in the CIE 1960 (UCS) space,
		Float u = (0.860117757f + 1.54118254e-4f * temperature + 1.28641212e-7f * temperature * temperature) / (1.0f + 8.42420235e-4f * temperature + 7.08145163e-7f * temperature * temperature);
		Float v = (0.317398726f + 4.22806245e-5f * temperature + 4.20481691e-8f * temperature * temperature) / (1.0f - 2.89741816e-5f * temperature + 1.61456053e-7f * temperature * temperature);

		//UCS to xyY
		Float x = 3.0f * u / (2.0f * u - 8.0f * v + 4.0f);
		Float y = 2.0f * v / (2.0f * u - 8.0f * v + 4.0f);
		Float z = 1.0f - x - y;

		//xyY to XYZ
		Float Y = 1.0f;
		Float X = Y / y * x;
		Float Z = Y / y * z;

		// XYZ to RGB - BT.709
		Float R = 3.2404542f * X + -1.5371385f * Y + -0.4985314f * Z;
		Float G = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
		Float B = 0.0556434f * X + -0.2040259f * Y + 1.0572252f * Z;
		return Color(R, G, B);
	}
}