#pragma once
#include <vector>
#include <array>
#include <string>
#include "GfxDefines.h"
#include "GfxDevice.h"
#include "../Utilities/HashMap.h"
#if GPU_MULTITHREADED
#include <mutex>
#endif

namespace adria
{
	struct Timestamp
	{
		FLOAT time_in_ms;
		std::string name;
	};

	class GfxBuffer;

	class GPUProfiler
	{
		static constexpr UINT64 FRAME_COUNT = GfxDevice::BackbufferCount();
		static constexpr UINT64 MAX_PROFILES = 64;

		struct QueryData
		{
			bool query_started = false;
			bool query_finished = false;
			ID3D12GraphicsCommandList* cmd_list = nullptr;
		};

	public:

		static GPUProfiler& Get()
		{
			static GPUProfiler gpu_profiler;
			return gpu_profiler;
		}
		void Init(GfxDevice* gfx);
		void Destroy();

		void NewFrame();
		void BeginProfileScope(ID3D12GraphicsCommandList* cmd_list, char const* name);
		void EndProfileScope(ID3D12GraphicsCommandList* cmd_list, char const* name);
		std::vector<Timestamp> GetProfilerResults(ID3D12GraphicsCommandList* cmd_list);

	private:
		GfxDevice* gfx = nullptr;
		ArcPtr<ID3D12QueryHeap> query_heap;
		std::unique_ptr<GfxBuffer> query_readback_buffer;

		std::array<QueryData, MAX_PROFILES> query_data;
		HashMap<std::string, uint32> name_to_index_map;
#if GPU_MULTITHREADED
		mutable std::mutex map_mutex;
		std::atomic_uint scope_counter = 0;
#else 
		uint32_t scope_counter = 0;
#endif
	};

	struct GPUProfileScope
	{
		GPUProfileScope(ID3D12GraphicsCommandList* _cmd_list, char const* _name)
			: name{ _name }, cmd_list{ _cmd_list }
		{
			GPUProfiler::Get().BeginProfileScope(cmd_list, name.c_str());
		}

		~GPUProfileScope()
		{
			GPUProfiler::Get().EndProfileScope(cmd_list, name.c_str());
		}

		ID3D12GraphicsCommandList* cmd_list;
		std::string name;
	};

#if GPU_PROFILING
	#define GPU_PROFILE_SCOPE(cmd_list, name) GPUProfileScope block(cmd_list, name)
	#define CONDITIONAL_GPU_PROFILE_SCOPE(cmd_list, name, cond) std::unique_ptr<GPUProfileScope> scoped_profile = nullptr; \
																	if(cond) scoped_profile = std::make_unique<ScopedGPUProfileBlock>(cmd_list, name)
#else 
	#define GPU_PROFILE_SCOPE(cmd_list, name) 
	#define CONDITIONAL_GPU_PROFILE_SCOPE(cmd_list, name, cond) 
#endif
}