#pragma once
#include "../Utilities/EnumUtil.h"

namespace adria
{
	enum ProfilerFlags
	{
		ProfilerFlag_None = 0x00,
		ProfilerFlag_Frame = 0x01,
		ProfilerFlag_GBuffer = 0x02,
		ProfilerFlag_COUNT = 3
	};

	static std::string ToString(ProfilerFlags flag)
	{
		switch (flag)
		{
		case ProfilerFlag_Frame:
			return "Frame Pass";
		case ProfilerFlag_GBuffer:
			return "GBuffer Pass";
		case ProfilerFlag_None:
		default:
			return "";
		}
		return "";
	}

	DEFINE_ENUM_BIT_OPERATORS(ProfilerFlags)


}