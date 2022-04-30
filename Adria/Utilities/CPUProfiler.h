#pragma once
#include <unordered_map>
#include <string>
#include <optional>

namespace adria
{

	class CPUProfiler
	{
		struct TimingData
		{
			std::optional<long long> start_ms;
			std::optional<long long> end_ms;
		};

	public:
		explicit CPUProfiler(size_t max_blocks = 16);
		~CPUProfiler() = default;

		void BeginProfileBlock(char const* name);
		void EndProfileBlock(char const* name);

		[[maybe_unused]] std::vector<std::string> GetProfilingResults(bool log = true);

	private:
		std::unordered_map<std::string, TimingData> cpu_time_map;
	};

	struct ScopedCPUProfileBlock
	{
		ScopedCPUProfileBlock(char const* name, CPUProfiler& prof)
			: name(name), prof(prof)
		{
			prof.BeginProfileBlock(name);
		}

		~ScopedCPUProfileBlock()
		{
			prof.EndProfileBlock(name);
		}

		char const* name;
		CPUProfiler& prof;
	};

#define SCOPED_CPU_PROFILE_BLOCK(profiler, name) ScopedCPUProfileBlock block(name, profiler)
#define SCOPED_CPU_PROFILE_BLOCK_ON_CONDITION(profiler, name, cond) std::unique_ptr<ScopedCPUProfileBlock> scoped_profile = nullptr; \
																					  if(cond) scoped_profile = std::make_unique<ScopedCPUProfileBlock>(name, profiler)
}