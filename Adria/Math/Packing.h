#pragma once
#include "../Core/Definitions.h"

namespace adria
{
	template<typename T>
	inline T Clamp(T value, T min = 0, T max = 1)
	{
		return value > max ? max : (value < min ? min : value);
	}

	inline uint32 PackToUint(float32 r, float32 g, float32 b, float32 a = 1.0f)
	{
		uint32 output = 0;
		output |= (uint8)(Clamp(r) * 255.0f) << 24;
		output |= (uint8)(Clamp(g) * 255.0f) << 16;
		output |= (uint8)(Clamp(b) * 255.0f) << 8;
		output |= (uint8)(Clamp(a) * 255.0f) << 0;
		return output;
	}
	inline uint32 PackToUint(float32(&arr)[3])
	{
		uint32 output = 0;
		output |= (uint8)(Clamp(arr[0]) * 255.0f) << 24;
		output |= (uint8)(Clamp(arr[1]) * 255.0f) << 16;
		output |= (uint8)(Clamp(arr[2]) * 255.0f) << 8;
		output |= (uint8)(255.0f) << 0;
		return output;
	}
}