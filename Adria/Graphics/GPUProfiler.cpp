#include "GPUProfiler.h"
#include "Buffer.h"
#include "../Core/Macros.h"

namespace adria
{
	void GPUProfiler::Init(GraphicsDevice* _gfx)
	{
		gfx = _gfx;
		query_readback_buffer = std::make_unique<Buffer>(gfx, ReadBackBufferDesc(MAX_PROFILES * 2 * FRAME_COUNT * sizeof(UINT64)));
		D3D12_QUERY_HEAP_DESC heap_desc{};
		heap_desc.Count = MAX_PROFILES * 2;
		heap_desc.NodeMask = 0;
		heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		gfx->GetDevice()->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(query_heap.GetAddressOf()));
	}

	void GPUProfiler::Destroy()
	{
		query_heap.Reset();
		query_readback_buffer.reset();
		gfx = nullptr;
	}

	void GPUProfiler::NewFrame()
	{
		for (size_t i = 0; i < MAX_PROFILES; ++i)
		{
			QueryData& profile_data = query_data[i];
			profile_data.query_started = profile_data.query_finished = false;
			profile_data.cmd_list = nullptr;
		}
		name_to_index_map.clear();
		scope_counter = 0;
	}

	void GPUProfiler::BeginProfileScope(ID3D12GraphicsCommandList* cmd_list, char const* name)
	{
		uint32_t profile_index = scope_counter++;
#if GPU_MULTITHREADED
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
		UINT32 begin_query_index = UINT32(profile_index * 2);
		cmd_list->EndQuery(query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, begin_query_index);
		profile_data.query_started = true;
	}

	void GPUProfiler::EndProfileScope(ID3D12GraphicsCommandList* cmd_list, char const* name)
	{
		uint32 profile_index = -1;
#if GPU_MULTITHREADED
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
		UINT32 begin_query_index = UINT32(profile_index * 2);
		UINT32 end_query_index = UINT32(profile_index * 2 + 1);
		cmd_list->EndQuery(query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, end_query_index);
		profile_data.query_finished = true;
		profile_data.cmd_list = cmd_list;
	}

	std::vector<Timestamp> GPUProfiler::GetProfilerResults(ID3D12GraphicsCommandList* cmd_list)
	{
		UINT64 gpu_frequency = 0;
		gfx->GetTimestampFrequency(gpu_frequency);
		UINT64 current_backbuffer_index = gfx->BackbufferIndex();
		for (auto const& [_, index] : name_to_index_map)
		{
			ADRIA_ASSERT(index < MAX_PROFILES);
			QueryData& profile_data = query_data[index];
			if (profile_data.query_started && profile_data.query_finished)
			{
				UINT32 begin_query_index = UINT32(index * 2);
				UINT32 end_query_index = UINT32(index * 2 + 1);
				UINT64 readback_offset = ((current_backbuffer_index * MAX_PROFILES * 2) + begin_query_index) * sizeof(UINT64);
				ADRIA_ASSERT(profile_data.cmd_list);
				profile_data.cmd_list->ResolveQueryData(query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, begin_query_index, 2, query_readback_buffer->GetNative(), readback_offset);
			}
		}
		UINT64 const* query_timestamps = query_readback_buffer->GetMappedData<UINT64>();
		UINT64 const* frame_query_timestamps = query_timestamps + (current_backbuffer_index * MAX_PROFILES * 2);

		std::vector<Timestamp> results{};
		for (auto const& [name, index] : name_to_index_map)
		{
			ADRIA_ASSERT(index < MAX_PROFILES);
			QueryData& profile_data = query_data[index];
			if (profile_data.query_started && profile_data.query_finished)
			{
				UINT64 start_time = frame_query_timestamps[index * 2 + 0];
				UINT64 end_time = frame_query_timestamps[index * 2 + 1];

				UINT64 delta = end_time - start_time;
				FLOAT frequency = FLOAT(gpu_frequency);
				FLOAT time_ms = (delta / frequency) * 1000.0f;
				results.emplace_back(time_ms, name);
			}
		}
		return results;
	}
}
