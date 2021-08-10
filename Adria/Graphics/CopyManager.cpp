#include "CopyManager.h"
#include "../Core/Macros.h"

namespace adria
{

	CopyManager::CopyManager(ID3D12Device* device)
	{
		D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
		copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		copyQueueDesc.NodeMask = 0;
		HRESULT hr = device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&copy_queue));
		BREAK_IF_FAILED(hr);
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&copy_allocator));
		BREAK_IF_FAILED(hr);
		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copy_allocator.Get(), nullptr, IID_PPV_ARGS(&copy_cmd_list));
		BREAK_IF_FAILED(hr);
		hr = static_cast<ID3D12GraphicsCommandList*>(copy_cmd_list.Get())->Close();
		BREAK_IF_FAILED(hr);
		hr = copy_allocator->Reset();
		BREAK_IF_FAILED(hr);
		hr = static_cast<ID3D12GraphicsCommandList*>(copy_cmd_list.Get())->Reset(copy_allocator.Get(), nullptr);
		BREAK_IF_FAILED(hr);
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&copy_fence));
		BREAK_IF_FAILED(hr);
		copy_fence_event = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);
		copy_fence_value = 1;
	}

	CopyManager::~CopyManager()
	{
		CloseHandle(copy_fence_event);
	}

	ID3D12GraphicsCommandList* CopyManager::CopyCmdList() const
	{
		return copy_cmd_list.Get();
	}

	void CopyManager::Wait()
	{
		copy_cmd_list->Close();

		ID3D12CommandList* cmd_lists[] = { static_cast<ID3D12CommandList*>(copy_cmd_list.Get()) };

		copy_queue->ExecuteCommandLists(1, cmd_lists);

		// Signal and increment the fence value.
		UINT64 fenceToWaitFor = copy_fence_value;
		HRESULT result = copy_queue->Signal(copy_fence.Get(), fenceToWaitFor);
		BREAK_IF_FAILED(result);
		copy_fence_value++;

		// Wait until the GPU is done copying.
		if (copy_fence->GetCompletedValue() < fenceToWaitFor)
		{
			result = copy_fence->SetEventOnCompletion(fenceToWaitFor, copy_fence_event);
			WaitForSingleObject(copy_fence_event, INFINITE);
		}

		result = copy_allocator->Reset();
		BREAK_IF_FAILED(result);
		result = copy_cmd_list->Reset(copy_allocator.Get(), nullptr);
		BREAK_IF_FAILED(result);
	}

}
