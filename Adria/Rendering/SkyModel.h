#pragma once
#include <DirectXMath.h>
#include "Core/CoreTypes.h"
#include <array>

namespace adria
{

	enum ESkyParams : uint16
	{
		ESkyParam_A = 0,
		ESkyParam_B,
		ESkyParam_C,
		ESkyParam_D,
		ESkyParam_E,
		ESkyParam_F,
		ESkyParam_G,
		ESkyParam_I,
		ESkyParam_H,
		ESkyParam_Z,
		ESkyParam_Count
	};

	using SkyParameters = std::array<DirectX::XMFLOAT3, ESkyParam_Count>;

	SkyParameters CalculateSkyParameters(float turbidity, float albedo, DirectX::XMFLOAT3 sun_direction);
}