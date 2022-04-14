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
		RT_Shadows,
		RT_AmbientOcclusion,
		RT_Reflections,
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
		case EProfilerBlock::RT_Shadows:
			return "RTS";
		case EProfilerBlock::RT_AmbientOcclusion:
			return "RTAO";
		case EProfilerBlock::RT_Reflections:
			return "RTR";
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
		bool profile_rts = false;
		bool profile_rtao = false;
		bool profile_rtr = false;
	};

	inline constexpr ProfilerSettings NO_PROFILING = ProfilerSettings{};

}