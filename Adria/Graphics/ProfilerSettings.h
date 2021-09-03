#pragma once
#include <string>
#include "../Utilities/EnumUtil.h"

namespace adria
{
	enum class ProfilerBlock
	{
		eFrame,
		eGBufferPass,
		eCount
	};

	static std::string ToString(ProfilerBlock profile_block)
	{
		switch (profile_block)
		{
		case ProfilerBlock::eFrame:
			return "Frame Pass";
		case ProfilerBlock::eGBufferPass:
			return "GBuffer Pass";
		default:
			return "";
		}
		return "";
	}

	struct ProfilerSettings
	{
		bool profile_frame = false;
		bool profile_gbuffer_pass = false;
	};

	inline constexpr ProfilerSettings NO_PROFILING = ProfilerSettings{};

}