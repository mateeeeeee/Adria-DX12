#pragma once
#include <string>
#include "../Utilities/EnumUtil.h"

namespace adria
{
	enum class ProfilerBlock
	{
		eFrame,
		eGBufferPass,
		eDeferredPass,
		eForwardPass,
		ePostprocessing,
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
		case ProfilerBlock::eDeferredPass:
			return "Deferred Pass";
		case ProfilerBlock::eForwardPass:
			return "Forward Pass";
		case ProfilerBlock::ePostprocessing:
			return "Postprocessing";
		default:
			return "";
		}
		return "";
	}

	struct ProfilerSettings
	{
		bool profile_frame = false;
		bool profile_gbuffer_pass = false;
		bool profile_deferred_pass = false;
		bool profile_forward_pass = false;
		bool profile_postprocessing = false;
	};

	inline constexpr ProfilerSettings NO_PROFILING = ProfilerSettings{};

}