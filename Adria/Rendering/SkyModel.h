#pragma once
#include <array>

namespace adria
{

	enum SkyParams : uint16
	{
		SkyParam_A = 0,
		SkyParam_B,
		SkyParam_C,
		SkyParam_D,
		SkyParam_E,
		SkyParam_F,
		SkyParam_G,
		SkyParam_I,
		SkyParam_H,
		SkyParam_Z,
		SkyParam_Count
	};

	using SkyParameters = std::array<Vector3, SkyParam_Count>;

	SkyParameters CalculateSkyParameters(float turbidity, float albedo, Vector3 const& sun_direction);
}