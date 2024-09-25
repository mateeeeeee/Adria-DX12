#pragma once
#include "Utilities/EnumUtil.h"

namespace adria
{
	enum GfxShadingRate1D : uint32
	{
		GfxShadingRate1D_1X = 0x01,
		GfxShadingRate1D_2X = 0x02,
		GfxShadingRate1D_4X = 0x04
	};

	static constexpr uint32 SHADING_RATE_SHIFT = 3;

	enum class GfxVariableShadingMode : uint32
	{
		None = 0,
		PerDraw,
		Image
	};

	enum GfxShadingRate : uint32
	{
		GfxShadingRate_1X1 = (GfxShadingRate1D_1X << SHADING_RATE_SHIFT) | GfxShadingRate1D_1X,
		GfxShadingRate_1X2 = (GfxShadingRate1D_1X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_1X4 = (GfxShadingRate1D_1X << SHADING_RATE_SHIFT) | GfxShadingRate1D_4X,
		GfxShadingRate_2X1 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_1X,
		GfxShadingRate_2X2 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_2X4 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_4X,
		GfxShadingRate_4X1 = (GfxShadingRate1D_4X << SHADING_RATE_SHIFT) | GfxShadingRate1D_1X,
		GfxShadingRate_4X2 = (GfxShadingRate1D_4X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_4X4 = (GfxShadingRate1D_4X << SHADING_RATE_SHIFT) | GfxShadingRate1D_4X
	};

	enum class GfxShadingRateCombiner : uint32
	{
		Passthrough = 1 << 0,
		Override = 1 << 1,   
		Min = 1 << 2,   
		Max = 1 << 3,   
		Sum = 1 << 4,   
		Mul = 1 << 5    
	};
	ENABLE_ENUM_BIT_OPERATORS(GfxShadingRateCombiner);

	static constexpr uint32 MAX_SHADING_RATES = 9;

	struct GfxShadingRateFeatureInfo
	{
		bool                   additional_shading_rates_supported = false;
		GfxShadingRate         shading_rates[MAX_SHADING_RATES];
		uint32                 number_of_shading_rates = 0;
		GfxShadingRateCombiner combiners;
		uint32                 min_tile_size[2];
		uint32                 max_tile_size[2];
	};
}