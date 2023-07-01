#include "GPUProfiler.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "GfxQueryHeap.h"
#include "GfxBuffer.h"
#include "Core/Defines.h"

namespace adria
{
	void GPUProfiler::Init(GfxDevice* _gfx)
	{
		gfx = _gfx;
		query_readback_buffer = std::make_unique<GfxBuffer>(gfx, ReadBackBufferDesc(MAX_PROFILES * 2 * FRAME_COUNT * sizeof(UINT64)));

		GfxQueryHeapDesc query_heap_desc{};
		query_heap_desc.count = MAX_PROFILES * 2;
		query_heap_desc.type = GfxQueryType::Timestamp;
		query_heap = std::make_unique<GfxQueryHeap>(gfx, query_heap_desc);
	}

	void GPUProfiler::Destroy()
	{
		query_heap.reset();
		query_readback_buffer.reset();
		gfx = nullptr;
	}

	void GPUProfiler::NewFrame()
	{
		for (auto& profile_data : query_data)
		{
			profile_data.query_started = profile_data.query_finished = false;
			profile_data.cmd_list = nullptr;
		}
		name_to_index_map.clear();
		scope_counter = 0;
	}

	void GPUProfiler::BeginProfileScope(GfxCommandList* cmd_list, char const* name)
	{
		uint32_t profile_index = scope_counter++;
#if GFX_MULTITHREADED
		{
			std::scoped_lock(map_mutex);
			name_to_index_map[name] = profile_index;
		}
#else
		name_to_index_map[name] = profile_index;
#endif
		QueryData& profile_data = query_data[profile_index];
		ADRIA_ASSERT(profile_data.query_started == false);
		ADRIA_ASSERT(profile_data.query_finished == false);
		uint32 begin_query_index = uint32(profile_index * 2);
		cmd_list->BeginQuery(*query_heap, begin_query_index);
		profile_data.query_started = true;
		profile_data.cmd_list = cmd_list;
	}

	void GPUProfiler::EndProfileScope(char const* name)
	{
		uint32 profile_index = -1;
#if GFX_MULTITHREADED
		{
			std::scoped_lock(map_mutex);
			profile_index = name_to_index_map[name];
		}
#else
		profile_index = name_to_index_map[name];
#endif
		ADRIA_ASSERT(profile_index != uint32(-1));
		QueryData& profile_data = query_data[profile_index];
		ADRIA_ASSERT(profile_data.query_started == true);
		ADRIA_ASSERT(profile_data.query_finished == false);
		uint32 begin_query_index = uint32(profile_index * 2);
		uint32 end_query_index = uint32(profile_index * 2 + 1);
		profile_data.cmd_list->EndQuery(*query_heap, end_query_index);
		profile_data.query_finished = true;
	}

	std::vector<Timestamp> GPUProfiler::GetProfilerResults()
	{
		UINT64 gpu_frequency = 0;
		gfx->GetTimestampFrequency(gpu_frequency);
		UINT64 current_backbuffer_index = gfx->GetBackbufferIndex();
		for (auto const& [_, index] : name_to_index_map)
		{
			ADRIA_ASSERT(index < MAX_PROFILES);
			QueryData& profile_data = query_data[index];
			if (profile_data.query_started && profile_data.query_finished)
			{
				uint32 begin_query_index = uint32(index * 2);
				uint32 end_query_index = uint32(index * 2 + 1);
				uint64 readback_offset = ((current_backbuffer_index * MAX_PROFILES * 2) + begin_query_index) * sizeof(uint64);
				ADRIA_ASSERT(profile_data.cmd_list);
				profile_data.cmd_list->ResolveQueryData(*query_heap, begin_query_index, 2, *query_readback_buffer, readback_offset);
			}
		}
		uint64 const* query_timestamps = query_readback_buffer->GetMappedData<uint64>();
		uint64 const* frame_query_timestamps = query_timestamps + (current_backbuffer_index * MAX_PROFILES * 2);

		std::vector<Timestamp> results{};
		results.reserve(name_to_index_map.size());
		for (auto const& [name, index] : name_to_index_map)
		{
			ADRIA_ASSERT(index < MAX_PROFILES);
			QueryData& profile_data = query_data[index];
			if (profile_data.query_started && profile_data.query_finished)
			{
				uint64 start_time = frame_query_timestamps[index * 2 + 0];
				uint64 end_time = frame_query_timestamps[index * 2 + 1];

				uint64 delta = end_time - start_time;
				float frequency = float(gpu_frequency);
				float time_ms = (delta / frequency) * 1000.0f;
				results.emplace_back(time_ms, name);
			}
		}
		return results;
	}

	GPUProfiler::GPUProfiler(){}
	GPUProfiler::~GPUProfiler(){}

}
