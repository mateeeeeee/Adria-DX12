#pragma once
#include "../Core/Definitions.h"

namespace adria
{
	template<typename T>
	inline T Clamp(T value, T min = 0, T max = 1)
	{
		return value > max ? max : (value < min ? min : value);
	}

	uint32 PackToUint(float32 r, float32 g, float32 b, float32 a = 1.0f);
	uint32 PackToUint(float32(&arr)[3]);

	uint32 PackTwoFloatsToUint32(float32 x, float32 y);
	uint64 PackFourFloatsToUint64(float32 x, float32 y, float32 z, float32 w);
}