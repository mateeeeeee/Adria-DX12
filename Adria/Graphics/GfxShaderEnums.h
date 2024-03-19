#pragma once

namespace adria
{
	enum class GfxShaderStage
	{
		VS,
		PS,
		HS,
		DS,
		GS,
		CS,
		LIB,
		MS,
		AS,
		ShaderCount
	};
	enum GfxShaderModel
	{
		SM_Unknown,
		SM_6_0,
		SM_6_1,
		SM_6_2,
		SM_6_3,
		SM_6_4,
		SM_6_5,
		SM_6_6,
		SM_6_7,
		SM_6_8
	};
}