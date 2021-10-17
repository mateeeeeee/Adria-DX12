#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include "ReadbackBuffer.h"
#include "ProfilerSettings.h"


namespace adria
{
	class Profiler
	{
		static constexpr UINT64 FRAME_COUNT = GraphicsCoreDX12::BackbufferCount();
		static constexpr UINT64 MAX_PROFILES = static_cast<UINT64>(EProfilerBlock::Count);

		struct QueryData
		{
			bool query_started = false;
			bool query_finished = false;
		};

	public:

		Profiler(GraphicsCoreDX12* gfx);

		void BeginProfileBlock(ID3D12GraphicsCommandList* cmd_list, EProfilerBlock block);

		void EndProfileBlock(ID3D12GraphicsCommandList* cmd_list, EProfilerBlock block);

		std::vector<std::string> GetProfilerResults(ID3D12GraphicsCommandList* cmd_list, bool log_results = false);

	private:
		GraphicsCoreDX12* gfx;
		std::array<QueryData, MAX_PROFILES> query_data;
		Microsoft::WRL::ComPtr<ID3D12QueryHeap> query_heap;
		ReadbackBuffer query_readback_buffer;
		UINT64 current_frame_index = 0;
	};

	struct ScopedProfileBlock
	{
		ScopedProfileBlock(Profiler& profiler, ID3D12GraphicsCommandList* cmd_list, EProfilerBlock block)
			: profiler{ profiler }, block{ block }, cmd_list{ cmd_list }
		{
			profiler.BeginProfileBlock(cmd_list, block);
		}

		~ScopedProfileBlock()
		{
			profiler.EndProfileBlock(cmd_list, block);
		}

		Profiler& profiler;
		ID3D12GraphicsCommandList* cmd_list;
		EProfilerBlock block;
	};

	#define DECLARE_SCOPED_PROFILE_BLOCK(profiler, cmd_list, block_id) ScopedProfileBlock block(profiler, context, block_id)
	#define DECLARE_SCOPED_PROFILE_BLOCK_ON_CONDITION(profiler, cmd_list, block_id, cond) std::unique_ptr<ScopedProfileBlock> scoped_profile = nullptr; \
																						  if(cond) scoped_profile = std::make_unique<ScopedProfileBlock>(profiler, cmd_list, block_id)
}