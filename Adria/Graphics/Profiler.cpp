#include "Profiler.h"
#include "../Core/Macros.h"
#include "../Logging/Logger.h"

namespace adria
{

	Profiler::Profiler(GraphicsCoreDX12* gfx) : gfx{ gfx }, query_readback_buffer(gfx->Device(), MAX_PROFILES * 2 * FRAME_COUNT * sizeof(UINT64))
	{
		D3D12_QUERY_HEAP_DESC heap_desc = { };
		heap_desc.Count = MAX_PROFILES * 2;
		heap_desc.NodeMask = 0;
		heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		gfx->Device()->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(&query_heap));
	}

	void Profiler::BeginProfileBlock(ID3D12GraphicsCommandList* cmd_list, ProfilerBlock block)
	{
		UINT32 profile_index = static_cast<UINT32>(block);
		QueryData& profile_data = query_data[profile_index];
		ADRIA_ASSERT(profile_data.query_started == false);
		ADRIA_ASSERT(profile_data.query_finished == false);
		UINT32 begin_query_index = UINT32(profile_index * 2);
		cmd_list->EndQuery(query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, begin_query_index);
		profile_data.query_started = true;
	}

	void Profiler::EndProfileBlock(ID3D12GraphicsCommandList* cmd_list, ProfilerBlock block)
	{
		UINT32 profile_index = static_cast<UINT32>(block);
		QueryData& profile_data = query_data[profile_index];
		ADRIA_ASSERT(profile_data.query_started == true);
		ADRIA_ASSERT(profile_data.query_finished == false);
		UINT32 begin_query_index = UINT32(profile_index * 2);
		UINT32 end_query_index = UINT32(profile_index * 2 + 1);
		cmd_list->EndQuery(query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, end_query_index);
		profile_data.query_finished = true;

		// Resolve the data
		UINT64 readback_offset = ((current_frame_index * MAX_PROFILES * 2) + begin_query_index) * sizeof(UINT64);
		cmd_list->ResolveQueryData(query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, begin_query_index, 2, query_readback_buffer.Resource(), readback_offset);
	}

	std::vector<std::string> Profiler::GetProfilerResults(ID3D12GraphicsCommandList* cmd_list, bool log_results /*= false*/)
	{
		UINT64 gpu_frequency = 0;
		gfx->GetTimestampFrequency(gpu_frequency);

		UINT64 const* query_timestamps = query_readback_buffer.Map<UINT64>();
		UINT64 const* frame_query_timestamps = query_timestamps + (current_frame_index * MAX_PROFILES * 2);

		std::vector<std::string> results{};
		for (size_t i = 0; i < MAX_PROFILES; ++i)
		{
			QueryData& profile_data = query_data[i];
			if (profile_data.query_started && profile_data.query_finished)
			{
				UINT64 start_time = frame_query_timestamps[i * 2 + 0];
				UINT64 end_time = frame_query_timestamps[i * 2 + 1];

				UINT64 delta = end_time - start_time;
				double frequency = double(gpu_frequency);
				double time_ms = (delta / frequency) * 1000.0;

				std::string time_ms_string = std::to_string(time_ms);
				std::string result = ToString(static_cast<ProfilerBlock>(i)) + " time: " + time_ms_string + "ms\n";
				results.push_back(result);
				if (log_results) Log::Info(result);
			}
			profile_data.query_started = profile_data.query_finished = false;
		}

		query_readback_buffer.Unmap();
		current_frame_index = (current_frame_index + 1) % FRAME_COUNT;

		return results;
	}


}

