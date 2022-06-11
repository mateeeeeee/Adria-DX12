#pragma once
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")


#include <memory>
#include <mutex>
#include <vector>
#include <array>
#include <queue>
#include <variant>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h> 

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"
#include "RingOnlineDescriptorAllocator.h"
#include "LinearOnlineDescriptorAllocator.h"
#include "OfflineDescriptorAllocator.h"
#include "LinearDynamicAllocator.h"
#include "Releasable.h"

namespace adria
{
	enum class EQueueType : uint8
	{
		Graphics,
		Compute,
	};

	struct GraphicsOptions
	{
		bool debug_layer = false;
		bool dred = false;
		bool gpu_validation = false;
	};

	class GraphicsDevice
	{
		static constexpr UINT BACKBUFFER_COUNT = 3;
		static constexpr UINT CMD_LIST_COUNT = 16;

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
		explicit GraphicsDevice(GraphicsOptions const&);
		GraphicsDevice(GraphicsDevice const&) = delete;
		GraphicsDevice(GraphicsDevice&&) = default;
		~GraphicsDevice();

		void WaitForGPU();

		void WaitOnQueue(EQueueType type, UINT64 fence_value);
		UINT64 SignalFromQueue(EQueueType type);

		void ResizeBackbuffer(UINT w, UINT h);
		UINT BackbufferIndex() const;
		UINT FrameIndex() const;
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

		D3D12_CPU_DESCRIPTOR_HANDLE AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE);
		void FreeOfflineDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE);
		void ReserveOnlineDescriptors(size_t reserve);
		RingOnlineDescriptorAllocator* GetOnlineDescriptorAllocator() const;
		LinearDynamicAllocator* GetDynamicAllocator() const;

		void GetTimestampFrequency(UINT64& frequency) const;

		static constexpr UINT BackbufferCount()
		{
			return BACKBUFFER_COUNT;
		}
	private:
		UINT width, height;
		UINT backbuffer_index;
		UINT last_backbuffer_index;
		UINT frame_index;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Device5> device = nullptr;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphics_queue = nullptr;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> compute_queue = nullptr;

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

		Microsoft::WRL::ComPtr<ID3D12Fence> compute_fences[BACKBUFFER_COUNT];
		HANDLE		 compute_fence_events[BACKBUFFER_COUNT];
		UINT64       compute_fence_values[BACKBUFFER_COUNT];

		Microsoft::WRL::ComPtr<ID3D12Fence> wait_fence = nullptr;
		HANDLE		 wait_event = nullptr;
		UINT64       wait_fence_value = 0;

		std::array< std::unique_ptr<OfflineDescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> offline_descriptor_allocators;

		std::unique_ptr<RingOnlineDescriptorAllocator> descriptor_allocator;
		std::vector<std::unique_ptr<LinearDynamicAllocator>> dynamic_allocators;

		Microsoft::WRL::ComPtr<ID3D12Fence> dred_fence = nullptr;
		HANDLE device_removed_event = nullptr;
		HANDLE wait_handle = nullptr;
	private:

		FrameResources& GetFrameResources();
		FrameResources const& GetFrameResources() const;

		void ExecuteGraphicsCommandLists();
		void ExecuteComputeCommandLists();

		void MoveToNextFrame();
		void ProcessReleaseQueue();
	};

}