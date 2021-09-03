#pragma once
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include <concepts>
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
    
	class GraphicsCoreDX12
	{
		static constexpr UINT BACKBUFFER_COUNT = 3;
        static constexpr UINT CMD_LIST_COUNT = 12;
		struct frame_resources_t
		{

			Microsoft::WRL::ComPtr<ID3D12Resource>	back_buffer = nullptr;
			D3D12_CPU_DESCRIPTOR_HANDLE				back_buffer_rtv{};
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      cmd_allocator = nullptr;
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>  cmd_list = nullptr;
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      cmd_allocators[CMD_LIST_COUNT] = {};
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>  cmd_lists[CMD_LIST_COUNT] = {};
            mutable std::atomic_uint cmd_list_index = 0;
		};

	public:

        GraphicsCoreDX12(void* window_handle);
        GraphicsCoreDX12(GraphicsCoreDX12 const&) = delete;
        GraphicsCoreDX12(GraphicsCoreDX12&&) = default;
        ~GraphicsCoreDX12();

        void WaitForGPU();
        void ResizeBackbuffer(UINT w, UINT h);
        void GetTimestampFrequency(UINT64& frequency) const;

        UINT BackbufferIndex() const;
        void SetBackbuffer(ID3D12GraphicsCommandList* cmd_list = nullptr);
        void ClearBackbuffer();
        void SwapBuffers(bool vsync = false);

        ID3D12Device5* Device() const;
        ID3D12GraphicsCommandList4* DefaultCommandList() const;
        ID3D12GraphicsCommandList4* NewCommandList() const;
        ID3D12GraphicsCommandList4* LastCommandList() const;

        void ResetCommandList();
        void ExecuteCommandList();
        
        D3D12MA::Allocator* Allocator() const;
        void AddToReleaseQueue(D3D12MA::Allocation* alloc);
        void AddToReleaseQueue(ID3D12Resource* resource);

        LinearDescriptorAllocator* DescriptorAllocator() const;
        LinearUploadBuffer* UploadBuffer() const;

		static constexpr UINT BackbufferCount()
		{
			return BACKBUFFER_COUNT;
		}

	private:

        UINT width, height;
        UINT backbuffer_index;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Device5> device = nullptr;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> direct_queue = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Fence> frame_fence = nullptr;
		HANDLE		 frame_fence_event = nullptr;
		UINT64       fence_values[BACKBUFFER_COUNT] = {};
		frame_resources_t frames[BACKBUFFER_COUNT];


        ReleasablePtr<D3D12MA::Allocator> allocator = nullptr;
        std::queue<ReleasableItem>  release_queue{};
        Microsoft::WRL::ComPtr<ID3D12Fence> release_queue_fence = nullptr;
        HANDLE		 release_queue_event = nullptr;
        UINT64       release_queue_fence_value = {};

        std::unique_ptr<DescriptorHeap> render_target_heap = nullptr;
        std::vector<std::unique_ptr<LinearDescriptorAllocator>> descriptor_allocators;
        std::vector<std::unique_ptr<LinearUploadBuffer>> upload_buffers;
    private:

        frame_resources_t& GetFrameResources();
        frame_resources_t const& GetFrameResources() const;
        void ExecuteCommandLists();
        void MoveToNextFrame();
        void ProcessReleaseQueue();
	};
}





