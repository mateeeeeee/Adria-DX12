#include "Packing.h"
#include <DirectXPackedVector.h>

namespace adria
{
	uint32 PackToUint(float(&arr)[3])
	{
		uint32 output = 0;
		output |= (uint8)(Clamp(arr[0]) * 255.0f) << 24;
		output |= (uint8)(Clamp(arr[1]) * 255.0f) << 16;
		output |= (uint8)(Clamp(arr[2]) * 255.0f) << 8;
		output |= (uint8)(255.0f) << 0;
		return output;
	}
	uint32 PackToUint(float r, float g, float b, float a /*= 1.0f*/)
	{
		uint32 output = 0;
		output |= (uint8)(Clamp(r) * 255.0f) << 24;
		output |= (uint8)(Clamp(g) * 255.0f) << 16;
		output |= (uint8)(Clamp(b) * 255.0f) << 8;
		output |= (uint8)(Clamp(a) * 255.0f) << 0;
		return output;
	}

	uint32 PackTwoFloatsToUint32(float x, float y)
	{
		DirectX::PackedVector::XMHALF2 packed(x, y);
		return packed.v;
	}
	uint64 PackFourFloatsToUint64(float x, float y, float z, float w)
	{
		DirectX::PackedVector::XMHALF4 packed(x, y, z, w);
		return packed.v;
	}

	uint32 PackTwoUint16ToUint32(uint16 value1, uint16 value2) 
	{
		value1 &= 0xFFFF;
		value2 &= 0xFFFF;
		uint32 packed_value = (static_cast<uint32>(value1) << 16) | static_cast<uint32>(value2);
		return packed_value;
	}
}
