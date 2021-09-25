#pragma once
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include <memory>
#include <mutex>
#include <vector>
#include <queue>
#include <variant>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h> 

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"
#include "RingDescriptorAllocator.h"
#include "LinearDescriptorAllocator.h"
#include "LinearUploadBuffer.h"
#include "Releasable.h"

namespace adria
{
	enum class QueueType : u8
	{
		eGraphics,
		eCompute,
		eCount
	};

	class GraphicsCoreDX12_v2
	{
		static constexpr UINT BACKBUFFER_COUNT = 3;
		static constexpr UINT CMD_LIST_COUNT = 8;

		struct FrameResources
		{
			Microsoft::WRL::ComPtr<ID3D12Resource>	back_buffer = nullptr;
			D3D12_CPU_DESCRIPTOR_HANDLE				back_buffer_rtv;

			Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      default_cmd_allocator;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>  default_cmd_list;

			Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      cmd_allocators[CMD_LIST_COUNT] = {};
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>  cmd_lists[CMD_LIST_COUNT] = {};
			mutable std::atomic_uint cmd_list_index = 0;

			Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      compute_cmd_allocators[CMD_LIST_COUNT] = {};
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>  compute_cmd_lists[CMD_LIST_COUNT] = {};
			mutable std::atomic_uint compute_cmd_list_index = 0;
		};

	public:
		GraphicsCoreDX12_v2(void* window_handle);
		GraphicsCoreDX12_v2(GraphicsCoreDX12_v2 const&) = delete;
		GraphicsCoreDX12_v2(GraphicsCoreDX12_v2&&) = default;
		~GraphicsCoreDX12_v2();

		void WaitForGPU();

		void WaitOnQueue(QueueType type)
		{
			switch (type)
			{
			case QueueType::eGraphics:
				graphics_queue->Wait(compute_fences[backbuffer_index].Get(), compute_fence_values[backbuffer_index]);
				++compute_fence_values[backbuffer_index];
				break;
			case QueueType::eCompute:
				compute_queue->Wait(graphics_fences[backbuffer_index].Get(), graphics_fence_values[backbuffer_index]);
				++graphics_fence_values[backbuffer_index];
				break;
			}
		}
		void SignalQueue(QueueType type)
		{
			switch (type)
			{
			case QueueType::eGraphics:
				graphics_queue->Signal(graphics_fences[backbuffer_index].Get(), graphics_fence_values[backbuffer_index]);
				break;
			case QueueType::eCompute:
				compute_queue->Signal(compute_fences[backbuffer_index].Get(), compute_fence_values[backbuffer_index]);
				break;
			}
		}

		void ResizeBackbuffer(UINT w, UINT h);
		UINT BackbufferIndex() const;
		void SetBackbuffer(ID3D12GraphicsCommandList* cmd_list = nullptr);
		void ClearBackbuffer();
		void SwapBuffers(bool vsync = false);

		ID3D12Device5* GetDevice() const;
		ID3D12GraphicsCommandList4* GetDefaultCommandList() const;
		ID3D12GraphicsCommandList4* GetNewGraphicsCommandList() const;
		ID3D12GraphicsCommandList4* GetLastGraphicsCommandList() const;
		ID3D12GraphicsCommandList4* GetNewComputeCommandList() const;
		ID3D12GraphicsCommandList4* GetLastComputeCommandList() const;

		void ResetDefaultCommandList();
		void ExecuteDefaultCommandList();

		D3D12MA::Allocator* GetAllocator() const;
		void AddToReleaseQueue(D3D12MA::Allocation* alloc);
		void AddToReleaseQueue(ID3D12Resource* resource);

		LinearDescriptorAllocator* GetDescriptorAllocator() const;
		LinearUploadBuffer* GetUploadBuffer() const;

		void GetTimestampFrequency(UINT64& frequency) const;

		static constexpr UINT BackbufferCount()
		{
			return BACKBUFFER_COUNT;
		}

	private:
		UINT width, height;
		UINT backbuffer_index;
		UINT last_backbuffer_index;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Device5> device = nullptr;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphics_queue = nullptr;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> compute_queue = nullptr;
		
		std::unique_ptr<DescriptorHeap> render_target_heap = nullptr;
		std::vector<std::unique_ptr<LinearDescriptorAllocator>> descriptor_allocators;
		std::vector<std::unique_ptr<LinearUploadBuffer>> upload_buffers;

		//release queue
		ReleasablePtr<D3D12MA::Allocator> allocator = nullptr;
		std::queue<ReleasableItem>  release_queue;
		Microsoft::WRL::ComPtr<ID3D12Fence> release_queue_fence = nullptr;
		HANDLE		 release_queue_event = nullptr;
		UINT64       release_queue_fence_value = 0;

		FrameResources frames[BACKBUFFER_COUNT];

		//sync objects
		Microsoft::WRL::ComPtr<ID3D12Fence> frame_fences[BACKBUFFER_COUNT];
		HANDLE		 frame_fence_events[BACKBUFFER_COUNT];
		UINT64       frame_fence_values[BACKBUFFER_COUNT];
		UINT64		 frame_fence_value;

		Microsoft::WRL::ComPtr<ID3D12Fence> graphics_fences[BACKBUFFER_COUNT];
		HANDLE		 graphics_fence_events[BACKBUFFER_COUNT];
		UINT64       graphics_fence_values[BACKBUFFER_COUNT];
		UINT64		 graphics_fence_value;

		Microsoft::WRL::ComPtr<ID3D12Fence> compute_fences[BACKBUFFER_COUNT];
		HANDLE		 compute_fence_events[BACKBUFFER_COUNT];
		UINT64       compute_fence_values[BACKBUFFER_COUNT];
		UINT64		 compute_fence_value;

	private:
		FrameResources& GetFrameResources();
		FrameResources const& GetFrameResources() const;

		void ExecuteGraphicsCommandLists();
		void ExecuteComputeCommandLists();

		void MoveToNextFrame();
		void ProcessReleaseQueue();
	};

}