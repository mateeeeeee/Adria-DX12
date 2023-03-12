#pragma once
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#include <memory>
#include <mutex>
#include <vector>
#include <array>
#include <queue>

#include <d3d12.h>
#include <dxgi1_6.h>

#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"

#include "GfxFence.h"
#include "RingOnlineDescriptorAllocator.h"
#include "LinearOnlineDescriptorAllocator.h"
#include "OfflineDescriptorAllocator.h"
#include "LinearDynamicAllocator.h"
#include "Releasable.h"

namespace adria
{
	enum class GfxQueueType : uint8
	{
		Graphics,
		Compute,
	};
	struct GfxOptions
	{
		bool debug_layer = false;
		bool dred = false;
		bool gpu_validation = false;
		bool pix = false;
	};
	struct GPUMemoryUsage
	{
		UINT64 usage;
		UINT64 budget;
	};

	class GfxDevice
	{
		static constexpr UINT BACKBUFFER_COUNT = 3;
		
		struct FrameResources
		{
			ArcPtr<ID3D12Resource>				back_buffer = nullptr;
			D3D12_CPU_DESCRIPTOR_HANDLE			back_buffer_rtv;

			ArcPtr<ID3D12CommandAllocator>      default_cmd_allocator;
			ArcPtr<ID3D12GraphicsCommandList4>  default_cmd_list;
		};


	public:
		explicit GfxDevice(GfxOptions const&);
		GfxDevice(GfxDevice const&) = delete;
		GfxDevice(GfxDevice&&) = default;
		~GfxDevice();

		void WaitForGPU();

		void WaitOnQueue(GfxQueueType type, UINT64 fence_value);
		UINT64 SignalFromQueue(GfxQueueType type);

		void ResizeBackbuffer(UINT w, UINT h);
		UINT BackbufferIndex() const;
		UINT FrameIndex() const;
		void SetBackbuffer(ID3D12GraphicsCommandList* cmd_list = nullptr); //todo: remove
		void ClearBackbuffer();
		void SwapBuffers(bool vsync = false);

		ID3D12Device5* GetDevice() const;
		ID3D12GraphicsCommandList4* GetCommandList() const;
		ID3D12RootSignature* GetCommonRootSignature() const;
		ID3D12Resource* GetBackbuffer() const;

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
		GPUMemoryUsage GetMemoryUsage() const
		{
			GPUMemoryUsage gpu_memory_usage{};
			D3D12MA::Budget budget;
			allocator->GetBudget(&budget, nullptr);
			gpu_memory_usage.budget = budget.BudgetBytes;
			gpu_memory_usage.usage = budget.UsageBytes;
			return gpu_memory_usage;
		}

		static constexpr UINT BackbufferCount()
		{
			return BACKBUFFER_COUNT;
		}
	private:
		UINT width, height;
		UINT backbuffer_index;
		UINT last_backbuffer_index;
		UINT frame_index;
		ArcPtr<IDXGISwapChain3> swap_chain = nullptr;
		ArcPtr<ID3D12Device5> device = nullptr;

		ArcPtr<ID3D12CommandQueue> graphics_queue = nullptr;
		ArcPtr<ID3D12CommandQueue> compute_queue = nullptr;

		//release queue
		ReleasablePtr<D3D12MA::Allocator> allocator = nullptr;
		std::queue<ReleasableItem>  release_queue;
		ArcPtr<ID3D12Fence> release_queue_fence = nullptr;
		HANDLE		 release_queue_event = nullptr;
		UINT64       release_queue_fence_value = 0;

		FrameResources frames[BACKBUFFER_COUNT];

		//sync objects
		GfxFence     frame_fence;
		UINT64		 frame_fence_value;
		UINT64       frame_fence_values[BACKBUFFER_COUNT];

		GfxFence     graphics_fence;
		UINT64       graphics_fence_values[BACKBUFFER_COUNT];

		GfxFence     compute_fence;
		UINT64       compute_fence_values[BACKBUFFER_COUNT];

		GfxFence     wait_fence;
		UINT64       wait_fence_value = 1;

		std::array<std::unique_ptr<OfflineDescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> offline_descriptor_allocators;

		std::unique_ptr<RingOnlineDescriptorAllocator> descriptor_allocator;
		std::vector<std::unique_ptr<LinearDynamicAllocator>> dynamic_allocators;
		std::unique_ptr<LinearDynamicAllocator> dynamic_allocator_before_rendering;

		struct DRED
		{
			DRED(GfxDevice* gfx);
			~DRED();

			GfxFence dred_fence;
			HANDLE   dred_wait_handle;
		};
		std::unique_ptr<DRED> dred;
		
		BOOL rendering_not_started = TRUE;

		ArcPtr<ID3D12RootSignature> global_root_signature = nullptr;
	private:
		void CreateCommonRootSignature();

		FrameResources& GetFrameResources();
		FrameResources const& GetFrameResources() const;

		void ExecuteCommandLists();
		void MoveToNextFrame();
		void ProcessReleaseQueue();
	};

}