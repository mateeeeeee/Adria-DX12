#include "GfxCommandQueue.h"
#include "GfxDevice.h"
#include "../Utilities/StringUtil.h"

namespace adria
{

	constexpr D3D12_COMMAND_LIST_TYPE GfxCommandQueue::GetCommandListType(GfxCommandQueueType type)
	{
		switch (type)
		{
		case GfxCommandQueueType::Graphics:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case GfxCommandQueueType::Compute:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case GfxCommandQueueType::Copy:
			return D3D12_COMMAND_LIST_TYPE_COPY;
		}
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

	bool GfxCommandQueue::Create(GfxDevice* gfx, GfxCommandQueueType type, char const* name /*= ""*/)
	{
		ID3D12Device* device = gfx->GetDevice();
		D3D12_COMMAND_QUEUE_DESC queue_desc{};
		queue_desc.Type = GetCommandListType(type);
		queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.NodeMask = 0;
		HRESULT hr = device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(command_queue.GetAddressOf()));
		if (FAILED(hr)) return false;
		command_queue->SetName(ToWideString(name).c_str());
		return true;
	}

	void GfxCommandQueue::ExecuteCommandLists(std::span<ID3D12CommandList*> cmd_lists)
	{
		command_queue->ExecuteCommandLists((uint32)cmd_lists.size(), cmd_lists.data());
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

