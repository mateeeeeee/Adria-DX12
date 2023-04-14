#pragma once
#include <vector>
#include <memory>
#include <array>
#include <string>
#include "GfxDefines.h"
#include "../Utilities/HashMap.h"
#include "../Utilities/Singleton.h"
#include "../Core/CoreTypes.h"
#if GPU_MULTITHREADED
#include <mutex>
#endif

namespace adria
{
	struct Timestamp
	{
		float time_in_ms;
		std::string name;
	};

	class GfxDevice;
	class GfxBuffer;
	class GfxQueryHeap;
	class GfxCommandList;

	class GPUProfiler : public Singleton<GPUProfiler>
	{
		friend class Singleton<GPUProfiler>;

		static constexpr uint64 FRAME_COUNT = GFX_BACKBUFFER_COUNT;
		static constexpr uint64 MAX_PROFILES = 64;

		struct QueryData
		{
			bool query_started = false;
			bool query_finished = false;
			GfxCommandList* cmd_list = nullptr;
		};

	public:

		void Init(GfxDevice* gfx);
		void Destroy();

		void NewFrame();
		void BeginProfileScope(GfxCommandList* cmd_list, char const* name);
		void EndProfileScope(char const* name);
		std::vector<Timestamp> GetProfilerResults();

	private:
		GfxDevice* gfx = nullptr;
		std::unique_ptr<GfxQueryHeap> query_heap;
		std::unique_ptr<GfxBuffer> query_readback_buffer;

		std::array<QueryData, MAX_PROFILES> query_data;
		HashMap<std::string, uint32> name_to_index_map;

#if GPU_MULTITHREADED
		mutable std::mutex map_mutex;
		std::atomic_uint scope_counter = 0;
#else
		uint32_t scope_counter = 0;
#endif

	private:
		GPUProfiler();
		~GPUProfiler();
	};
	#define g_GpuProfiler GPUProfiler::Get()

	struct GPUProfileScope
	{
		GPUProfileScope(GfxCommandList* _cmd_list, char const* _name)
			: name{ _name }, cmd_list{ _cmd_list }
		{
			g_GpuProfiler.BeginProfileScope(cmd_list, name.c_str());
		}

		~GPUProfileScope()
		{
			g_GpuProfiler.EndProfileScope(name.c_str());
		}

		GfxCommandList* cmd_list;
		std::string name;
	};

#if GFX_PROFILING
	#define GPU_PROFILE_SCOPE(cmd_list, name) GPUProfileScope block(cmd_list, name)
	#define CONDITIONAL_GPU_PROFILE_SCOPE(cmd_list, name, cond) std::unique_ptr<GPUProfileScope> scoped_profile = nullptr; \
																	if(cond) scoped_profile = std::make_unique<ScopedGPUProfileBlock>(cmd_list, name)
#else
	#define GPU_PROFILE_SCOPE(cmd_list, name)
	#define CONDITIONAL_GPU_PROFILE_SCOPE(cmd_list, name, cond)
#endif
	
}