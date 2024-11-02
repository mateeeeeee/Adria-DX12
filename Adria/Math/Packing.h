#pragma once

namespace adria
{
	template<typename T>
	inline T Clamp(T value, T min = 0, T max = 1)
	{
		return value > max ? max : (value < min ? min : value);
	}

	Uint32 PackToUint(float r, float g, float b, float a = 1.0f);
	Uint32 PackToUint(float(&arr)[3]);

	Uint32 PackTwoFloatsToUint32(float x, float y);
	Uint64 PackFourFloatsToUint64(float x, float y, float z, float w);

	Uint32 PackTwoUint16ToUint32(Uint16 value1, Uint16 value2);
}