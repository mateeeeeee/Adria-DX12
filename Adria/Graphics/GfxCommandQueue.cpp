#include "GfxCommandQueue.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "GfxCommandListPool.h"
#include "Utilities/StringUtil.h"

namespace adria
{
	bool GfxCommandQueue::Create(GfxDevice* gfx, GfxCommandListType type, char const* name)
	{
		ID3D12Device* device = gfx->GetDevice();
		D3D12_COMMAND_QUEUE_DESC queue_desc{};
		auto GetCmdListType = [](GfxCommandListType type)
		{
			switch (type)
			{
			case GfxCommandListType::Graphics:
				return D3D12_COMMAND_LIST_TYPE_DIRECT;
			case GfxCommandListType::Compute:
				return D3D12_COMMAND_LIST_TYPE_COMPUTE;
			case GfxCommandListType::Copy:
				return D3D12_COMMAND_LIST_TYPE_COPY;
			}
			return D3D12_COMMAND_LIST_TYPE_NONE;
		};
		queue_desc.Type = GetCmdListType(type);
		queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.NodeMask = 0;
		HRESULT hr = device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(command_queue.GetAddressOf()));
		if (FAILED(hr)) return false;
		command_queue->SetName(ToWideString(name).c_str());
		if(type != GfxCommandListType::Copy) command_queue->GetTimestampFrequency(&timestamp_frequency);
		return true;
	}

	void GfxCommandQueue::ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists)
	{
		if (cmd_lists.empty()) return;

		for (GfxCommandList* cmd_list : cmd_lists) cmd_list->WaitAll();

		std::vector<ID3D12CommandList*> d3d12_cmd_lists(cmd_lists.size());
		for (uint64 i = 0; i < d3d12_cmd_lists.size(); ++i) d3d12_cmd_lists[i] = cmd_lists[i]->GetNative();
		command_queue->ExecuteCommandLists((uint32)d3d12_cmd_lists.size(), d3d12_cmd_lists.data());

		for (GfxCommandList* cmd_list : cmd_lists) cmd_list->SignalAll();
	}

	void GfxCommandQueue::ExecuteCommandListPool(GfxCommandListPool& cmd_list_pool)
	{
		std::vector<GfxCommandList*> cmd_lists; cmd_lists.reserve(cmd_list_pool.cmd_lists.size());
		for (auto& cmd_list : cmd_list_pool.cmd_lists) cmd_lists.push_back(cmd_list.get());
		ExecuteCommandLists(cmd_lists);
	}

	void GfxCommandQueue::Signal(GfxFence& fence, uint64 fence_value)
	{
		command_queue->Signal(fence, fence_value);
	}

	void GfxCommandQueue::Wait(GfxFence& fence, uint64 fence_value)
	{
		command_queue->Wait(fence, fence_value);
	}

}

