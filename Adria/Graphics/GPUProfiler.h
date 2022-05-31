#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include "Buffer.h"
#include "ProfilerSettings.h"


namespace adria
{
	class GPUProfiler
	{
		static constexpr UINT64 FRAME_COUNT = GraphicsDevice::BackbufferCount();
		static constexpr UINT64 MAX_PROFILES = static_cast<UINT64>(EProfilerBlock::Count);

		struct QueryData
		{
			bool query_started = false;
			bool query_finished = false;
			ID3D12GraphicsCommandList* cmd_list = nullptr;
		};

	public:

		GPUProfiler(GraphicsDevice* gfx);

		void BeginProfileBlock(ID3D12GraphicsCommandList* cmd_list, EProfilerBlock block);

		void EndProfileBlock(ID3D12GraphicsCommandList* cmd_list, EProfilerBlock block);

		std::vector<std::string> GetProfilerResults(ID3D12GraphicsCommandList* cmd_list, bool log_results);

	private:
		GraphicsDevice* gfx;
		std::array<QueryData, MAX_PROFILES> query_data;
		Microsoft::WRL::ComPtr<ID3D12QueryHeap> query_heap;
		Buffer query_readback_buffer;
		UINT64 current_frame_index = 0;
	};

	struct ScopedGPUProfileBlock
	{
		ScopedGPUProfileBlock(GPUProfiler& profiler, ID3D12GraphicsCommandList* cmd_list, EProfilerBlock block)
			: profiler{ profiler }, block{ block }, cmd_list{ cmd_list }
		{
			profiler.BeginProfileBlock(cmd_list, block);
		}

		~ScopedGPUProfileBlock()
		{
			profiler.EndProfileBlock(cmd_list, block);
		}

		GPUProfiler& profiler;
		ID3D12GraphicsCommandList* cmd_list;
		EProfilerBlock block;
	};

	#define SCOPED_GPU_PROFILE_BLOCK(profiler, cmd_list, block_id) ScopedGPUProfileBlock block(profiler, context, block_id)
	#define SCOPED_GPU_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, block_id, cond) std::unique_ptr<ScopedGPUProfileBlock> scoped_profile = nullptr; \
																						  if(cond) scoped_profile = std::make_unique<ScopedGPUProfileBlock>(profiler, cmd_list, block_id)
}