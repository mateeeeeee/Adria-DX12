#include "Packing.h"
#include <DirectXPackedVector.h>

namespace adria
{
	uint32 PackToUint(float32(&arr)[3])
	{
		uint32 output = 0;
		output |= (uint8)(Clamp(arr[0]) * 255.0f) << 24;
		output |= (uint8)(Clamp(arr[1]) * 255.0f) << 16;
		output |= (uint8)(Clamp(arr[2]) * 255.0f) << 8;
		output |= (uint8)(255.0f) << 0;
		return output;
	}
	uint32 PackToUint(float32 r, float32 g, float32 b, float32 a /*= 1.0f*/)
	{
		uint32 output = 0;
		output |= (uint8)(Clamp(r) * 255.0f) << 24;
		output |= (uint8)(Clamp(g) * 255.0f) << 16;
		output |= (uint8)(Clamp(b) * 255.0f) << 8;
		output |= (uint8)(Clamp(a) * 255.0f) << 0;
		return output;
	}

	uint32 PackTwoFloatsToUint32(float32 x, float32 y)
	{
		DirectX::PackedVector::XMHALF2 packed(x, y);
		return packed.v;
	}
	uint64 PackFourFloatsToUint64(float32 x, float32 y, float32 z, float32 w)
	{
		DirectX::PackedVector::XMHALF4 packed(x, y, z, w);
		return packed.v;
	}
}
