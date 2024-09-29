#pragma once
#include "Utilities/EnumUtil.h"

namespace adria
{
	static constexpr uint32 SHADING_RATE_SHIFT = 3;
	static constexpr uint32 MAX_SHADING_RATES  = 7;
	static constexpr uint32 SHADING_RATE_COMBINER_COUNT = 2;
	

	enum GfxShadingRate1D : uint32
	{
		GfxShadingRate1D_1X = 0x01,
		GfxShadingRate1D_2X = 0x02,
		GfxShadingRate1D_4X = 0x04
	};

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
		GfxShadingRate_2X1 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_1X,
		GfxShadingRate_2X2 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_2X4 = (GfxShadingRate1D_2X << SHADING_RATE_SHIFT) | GfxShadingRate1D_4X,
		GfxShadingRate_4X2 = (GfxShadingRate1D_4X << SHADING_RATE_SHIFT) | GfxShadingRate1D_2X,
		GfxShadingRate_4X4 = (GfxShadingRate1D_4X << SHADING_RATE_SHIFT) | GfxShadingRate1D_4X
	};

	enum class GfxShadingRateCombiner : uint32
	{
		Passthrough,
		Override,   
		Min,
		Max,
		Sum,
	};

	class GfxTexture;
	struct GfxShadingRateInfo
	{
		GfxVariableShadingMode shading_mode = GfxVariableShadingMode::None;
		GfxShadingRate shading_rate = GfxShadingRate_1X1;
		GfxShadingRateCombiner shading_rate_combiner = GfxShadingRateCombiner::Passthrough;
		GfxTexture* shading_rate_image = nullptr;
	};

	inline D3D12_SHADING_RATE ToD3D12ShadingRate(GfxShadingRate shading_rate)
	{
		switch (shading_rate)
		{
		case GfxShadingRate_1X1: return D3D12_SHADING_RATE_1X1;
		case GfxShadingRate_1X2: return D3D12_SHADING_RATE_1X2;
		case GfxShadingRate_2X1: return D3D12_SHADING_RATE_2X1;
		case GfxShadingRate_2X2: return D3D12_SHADING_RATE_2X2;
		case GfxShadingRate_2X4: return D3D12_SHADING_RATE_2X4;
		case GfxShadingRate_4X2: return D3D12_SHADING_RATE_4X2;
		case GfxShadingRate_4X4: return D3D12_SHADING_RATE_4X4;
		}
		return D3D12_SHADING_RATE_1X1;
	}

	inline D3D12_SHADING_RATE_COMBINER ToD3D12ShadingRateCombiner(GfxShadingRateCombiner shading_rate_combiner)
	{
		switch (shading_rate_combiner)
		{
		case GfxShadingRateCombiner::Passthrough: return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
		case GfxShadingRateCombiner::Override:    return D3D12_SHADING_RATE_COMBINER_OVERRIDE;
		case GfxShadingRateCombiner::Min:		  return D3D12_SHADING_RATE_COMBINER_MIN;		  
		case GfxShadingRateCombiner::Max:		  return D3D12_SHADING_RATE_COMBINER_MAX;		  
		case GfxShadingRateCombiner::Sum:		  return D3D12_SHADING_RATE_COMBINER_SUM;		  
		}
		return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
	}
}