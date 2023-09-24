#include <vector>
#include <memory>
#include <array>
#include <string>
#if GFX_MULTITHREADED
#include <mutex>
#endif

#include "GfxProfiler.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "GfxQueryHeap.h"
#include "GfxBuffer.h"


namespace adria
{
	struct GfxProfiler::Impl
	{
		static constexpr uint64 FRAME_COUNT = GFX_BACKBUFFER_COUNT;
		static constexpr uint64 MAX_PROFILES = 256;

		struct QueryData
		{
			bool query_started = false;
			bool query_finished = false;
			GfxCommandList* cmd_list = nullptr;
		};

		GfxDevice* gfx = nullptr;
		std::unique_ptr<GfxQueryHeap> query_heap;
		std::unique_ptr<GfxBuffer> query_readback_buffer;

		std::array<QueryData, MAX_PROFILES> query_data;
		std::unordered_map<std::string, uint32> name_to_index_map;

#if GFX_MULTITHREADED
		mutable std::mutex map_mutex;
		std::atomic_uint scope_counter = 0;
#else
		uint32 scope_counter = 0;
#endif

		void Init(GfxDevice* _gfx)
		{
			gfx = _gfx;
			query_readback_buffer = gfx->CreateBuffer(ReadBackBufferDesc(MAX_PROFILES * 2 * FRAME_COUNT * sizeof(uint64)));

			GfxQueryHeapDesc query_heap_desc{};
			query_heap_desc.count = MAX_PROFILES * 2;
			query_heap_desc.type = GfxQueryType::Timestamp;
			query_heap = gfx->CreateQueryHeap(query_heap_desc);
		}
		void Destroy()
		{
			query_heap.reset();
			query_readback_buffer.reset();
			gfx = nullptr;
		}
		void NewFrame()
		{
			for (auto& profile_data : query_data)
			{
				profile_data.query_started = profile_data.query_finished = false;
				profile_data.cmd_list = nullptr;
			}
			name_to_index_map.clear();
			scope_counter = 0;
		}
		void BeginProfileScope(GfxCommandList* cmd_list, char const* name)
		{
			uint32 profile_index = scope_counter++;
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
		void EndProfileScope(char const* name)
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
		std::vector<GfxTimestamp> GetResults()
		{
			uint64 gpu_frequency = 0;
			gfx->GetTimestampFrequency(gpu_frequency);
			uint64 current_backbuffer_index = gfx->GetBackbufferIndex();
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

			std::vector<GfxTimestamp> results{};
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
	};

	void GfxProfiler::Initialize(GfxDevice* _gfx)
	{
		pimpl = std::make_unique<Impl>();
		pimpl->Init(_gfx);
	}

	void GfxProfiler::Destroy()
	{
		pimpl->Destroy();
		pimpl = nullptr;
	}

	void GfxProfiler::NewFrame()
	{
		pimpl->NewFrame();
	}

	void GfxProfiler::BeginProfileScope(GfxCommandList* cmd_list, char const* name)
	{
		pimpl->BeginProfileScope(cmd_list, name);
	}

	void GfxProfiler::EndProfileScope(char const* name)
	{
		pimpl->EndProfileScope(name);
	}

	std::vector<GfxTimestamp> GfxProfiler::GetResults()
	{
		return pimpl->GetResults();
	}

	GfxProfiler::GfxProfiler() {}
	GfxProfiler::~GfxProfiler() {}
}



