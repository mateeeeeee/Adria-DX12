#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include <unordered_map>
#include "ProfilerFlags.h"
#include "../Core/Macros.h"

namespace adria
{
	class Profiler
	{
		static constexpr UINT64 FRAME_COUNT = 3;
		static constexpr UINT64 MAX_PROFILES = 32;

		struct QueryData
		{
			UINT32 query_id;
			bool query_started = false;
			bool query_finished = false;
		};

	public:

		Profiler(ID3D12Device* device) : device{ device }
		{
			for (auto& query_heap : query_heaps)
			{
				D3D12_QUERY_HEAP_DESC heap_desc = { };
				heap_desc.Count = MAX_PROFILES * 2;
				heap_desc.NodeMask = 0;
				heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
				device->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(&query_heap));
			}
		}

		void AddProfileBlock(ProfilerFlags flag)
		{
			
		}

		void BeginProfileBlock(ID3D12GraphicsCommandList* cmd_list, ProfilerFlags flag)
		{
			
		}

		void EndProfileBlock(ID3D12GraphicsCommandList* cmd_list, ProfilerFlags flag)
		{

		}

		std::vector<std::string> GetProfilerResults(ID3D12GraphicsCommandList* cmd_list, bool log results = false)
		{
			
		}

	private:
		ID3D12Device* device;
		std::unordered_map<ProfilerFlags, QueryData> query_data_map;
		std::array<Microsoft::WRL::ComPtr<ID3D12QueryHeap>, FRAME_COUNT> query_heaps;
		UINT64 current_frame = 0;
	};
}