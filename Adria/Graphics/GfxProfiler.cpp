#if GFX_MULTITHREADED
#include <mutex>
#endif
#include "GfxProfiler.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "GfxQueryHeap.h"
#include "GfxBuffer.h"
#include "Logging/Logger.h"


namespace adria
{
	struct GfxProfiler::Impl
	{
		GfxDevice* gfx = nullptr;
		std::unique_ptr<GfxQueryHeap> query_heap;
		std::unique_ptr<GfxBuffer> query_readback_buffer;
#if GFX_MULTITHREADED
		std::mutex stack_mutex;
#endif
		GfxProfilerTreeAllocator profile_allocators[FRAME_COUNT];
		GfxProfilerTree profiler_trees[FRAME_COUNT];
		GfxProfilerTree* current_profiler_tree = nullptr;

		struct QueryData
		{
			GfxCommandList* cmd_list = nullptr;
			GfxProfilerTreeNode* tree_node = nullptr;
		};
		std::stack<QueryData> query_data;
		Uint32 scope_counter = 0;

		GfxProfiler::Impl() 
			: profiler_trees{ GfxProfilerTree(profile_allocators[0]), GfxProfilerTree(profile_allocators[1]), GfxProfilerTree(profile_allocators[2]) }
		{
		}

		void Init(GfxDevice* _gfx)
		{
			gfx = _gfx;
			query_readback_buffer = gfx->CreateBuffer(ReadBackBufferDesc(MAX_PROFILES * 2 * FRAME_COUNT * sizeof(Uint64)));

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
			ADRIA_ASSERT(query_data.empty()); 
			current_profiler_tree = &profiler_trees[gfx->GetBackbufferIndex()];
			current_profiler_tree->Clear();
			profile_allocators[gfx->GetBackbufferIndex()].Reset();
			scope_counter = 0;
		}
		void BeginProfileScope(GfxCommandList* cmd_list, Char const* name)
		{
#if GFX_MULTITHREADED
			std::lock_guard lock(stack_mutex);
#endif
			Uint32 profile_index = scope_counter++;
			GfxProfilerTreeNode* tree_node = nullptr;
			if (!query_data.empty())
			{
				QueryData& parent_data = query_data.top();
				tree_node = parent_data.tree_node->EmplaceChild(name, cmd_list, profile_index, 0.0f);
			}
			else
			{
				ADRIA_ASSERT(current_profiler_tree->GetRoot() == nullptr);
				current_profiler_tree->EmplaceRoot(name, cmd_list, profile_index, 0.0f);
				tree_node = current_profiler_tree->GetRoot();
			}
			QueryData& scope_data = query_data.emplace(cmd_list, tree_node);
			Uint32 begin_query_index = profile_index * 2;
			cmd_list->BeginQuery(*query_heap, begin_query_index);
		}
		void EndProfileScope(GfxCommandList* cmd_list)
		{
			ADRIA_ASSERT(!query_data.empty());
#if GFX_MULTITHREADED
			std::lock_guard lock(stack_mutex);
#endif
			QueryData& scope_data = query_data.top();
			ADRIA_ASSERT(scope_data.cmd_list == cmd_list);
			query_data.pop();

			Uint32 profile_index = scope_data.tree_node->GetData().index;
			Uint32 end_query_index = profile_index * 2 + 1;
			cmd_list->EndQuery(*query_heap, end_query_index);
		}
		GfxProfilerTree const* GetProfilerTree() const
		{
			Uint64 gpu_frequency = 0;
			gfx->GetTimestampFrequency(gpu_frequency);
			Uint64 current_backbuffer_index = gfx->GetBackbufferIndex();

			current_profiler_tree->TraversePreOrder([this, current_backbuffer_index](GfxProfilerTreeNode* node)
			{
					Uint32 const index = node->GetData().index;
					GfxCommandList* cmd_list = node->GetData().cmd_list;
					ADRIA_ASSERT(index < MAX_PROFILES);
					ADRIA_ASSERT(cmd_list);
					Uint32 const begin_query_index = index * 2;
					Uint32 const end_query_index = index * 2 + 1;
					Uint64 readback_offset = ((current_backbuffer_index * MAX_PROFILES * 2) + begin_query_index) * sizeof(Uint64);
					cmd_list->ResolveQueryData(*query_heap, begin_query_index, 2, *query_readback_buffer, readback_offset);
			});

			Uint64 const* query_timestamps = query_readback_buffer->GetMappedData<Uint64>();
			Uint64 const* frame_query_timestamps = query_timestamps + (current_backbuffer_index * MAX_PROFILES * 2);

			current_profiler_tree->TraversePreOrder([this, gpu_frequency, frame_query_timestamps](GfxProfilerTreeNode* node)
				{
					Uint32 const index = node->GetData().index;
					Uint64 start_time = frame_query_timestamps[index * 2 + 0];
					Uint64 end_time = frame_query_timestamps[index * 2 + 1];
					Uint64 delta = end_time - start_time;
					Float frequency = Float(gpu_frequency);
					node->GetData().time = (delta / frequency) * 1000.0f;
				});
			return current_profiler_tree;
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

	void GfxProfiler::BeginProfileScope(GfxCommandList* cmd_list, Char const* name)
	{
		pimpl->BeginProfileScope(cmd_list, name);
	}

	void GfxProfiler::EndProfileScope(GfxCommandList* cmd_list)
	{
		pimpl->EndProfileScope(cmd_list);
	}

	GfxProfilerTree const* GfxProfiler::GetProfilerTree() const
	{
		return pimpl->GetProfilerTree();
	}

	//std::vector<GfxTimestamp> GfxProfiler::GetResults()
	//{
	//	return pimpl->GetResults();
	//}

	GfxProfiler::GfxProfiler() {}
	GfxProfiler::~GfxProfiler() {}
}



