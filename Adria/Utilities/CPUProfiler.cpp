#include "CPUProfiler.h"
#include "Timer.h"
#include "../Logging/Logger.h"

namespace adria
{

	namespace
	{
		Timer<> cpu_profiler_timer;
	}

	void CPUProfiler::BeginProfileBlock(char const* name)
	{
		cpu_time_map[name].start_ms = cpu_profiler_timer.Elapsed();
	}

	void CPUProfiler::EndProfileBlock(char const* name)
	{
		cpu_time_map[name].end_ms = cpu_profiler_timer.Elapsed();
	}

	std::vector<std::string> CPUProfiler::GetProfilingResults(bool log /*= true*/)
	{
		std::vector<std::string> results{};
		for (auto&& [name, data] : cpu_time_map)
		{
			if (!data.start_ms || !data.end_ms)
			{
				ADRIA_LOG(WARNING, "Either BeginProfileBlock or EndProfileBlock was not called for CPU profile block %s! Skipping...", name.c_str());
				continue;
			}
			long long const ms = data.end_ms.value() - data.start_ms.value();
			std::string result = name + " time: " + std::to_string(ms) + "ms";
			results.push_back(result);

			if (log) ADRIA_LOG(INFO, result.c_str());
		}
		return results;
	}

	CPUProfiler::CPUProfiler(size_t max_blocks /*= 16*/)
	{
		cpu_time_map.reserve(max_blocks);
	}


}

