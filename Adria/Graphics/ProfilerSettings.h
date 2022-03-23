#pragma once
#include <string>
#include "../Utilities/EnumUtil.h"

namespace adria
{
	enum class EProfilerBlock
	{
		Frame,
		GBufferPass,
		DecalPass,
		DeferredPass,
		ForwardPass,
		ParticlesPass,
		Postprocessing,
		Count
	};

	static constexpr std::string ToString(EProfilerBlock profile_block)
	{
		switch (profile_block)
		{
		case EProfilerBlock::Frame:
			return "Frame Pass";
		case EProfilerBlock::GBufferPass:
			return "GBuffer Pass";
		case EProfilerBlock::DecalPass:
			return "Decal Pass";
		case EProfilerBlock::DeferredPass:
			return "Deferred Pass";
		case EProfilerBlock::ForwardPass:
			return "Forward Pass";
		case EProfilerBlock::ParticlesPass:
			return "Particles Pass";
		case EProfilerBlock::Postprocessing:
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
		bool profile_decal_pass = false;
		bool profile_deferred_pass = false;
		bool profile_forward_pass = false;
		bool profile_particles_pass = false;
		bool profile_postprocessing = false;
	};

	inline constexpr ProfilerSettings NO_PROFILING = ProfilerSettings{};

}