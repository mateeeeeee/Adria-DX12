#pragma once
#include <DirectXMath.h>
#include "../Core/Definitions.h"
#include <array>

namespace adria
{

	enum class ESkyParams : uint8
	{
		A = 0,
		B,
		C,
		D,
		E,
		F,
		G,
		I,
		H,
		Z,
		Count
	};

	using SkyParameters = std::array<DirectX::XMFLOAT3, (size_t)ESkyParams::Count>;

	SkyParameters CalculateSkyParameters(float32 turbidity, float32 albedo, DirectX::XMFLOAT3 sun_direction);
}