#pragma once
#include "../Core/CoreTypes.h"

namespace adria
{
	template<typename T>
	inline T Clamp(T value, T min = 0, T max = 1)
	{
		return value > max ? max : (value < min ? min : value);
	}

	uint32 PackToUint(float r, float g, float b, float a = 1.0f);
	uint32 PackToUint(float(&arr)[3]);

	uint32 PackTwoFloatsToUint32(float x, float y);
	uint64 PackFourFloatsToUint64(float x, float y, float z, float w);
}