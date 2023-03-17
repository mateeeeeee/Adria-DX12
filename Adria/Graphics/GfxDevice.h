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
#include "GfxCommandQueue.h"
#include "CommandSignature.h"
#include "RingGPUDescriptorAllocator.h"
#include "LinearGPUDescriptorAllocator.h"
#include "CPUDescriptorAllocator.h"
#include "LinearDynamicAllocator.h"
#include "../Utilities/Releasable.h"

namespace adria
{
	class GfxSwapchain;
	class GfxCommandList;


	struct GfxOptions
	{
		bool debug_layer = false;
		bool dred = false;
		bool gpu_validation = false;
		bool pix = false;
	};
	struct GPUMemoryUsage
	{
		uint64 usage;
		uint64 budget;
	};

	class GfxDevice
	{
		static constexpr uint32 BACKBUFFER_COUNT = 3;

		struct FrameResources
		{
			ArcPtr<ID3D12CommandAllocator>      cmd_allocator;
			ArcPtr<ID3D12GraphicsCommandList4>  cmd_list;
		};

		struct ReleasableItem
		{
			std::unique_ptr<ReleasableObject> obj;
			uint64 fence_value;

			ReleasableItem(ReleasableObject* obj, size_t fence_value) : obj(obj), fence_value(fence_value) {}
		};

	public:
		explicit GfxDevice(GfxOptions const&);
		GfxDevice(GfxDevice const&) = delete;
		GfxDevice(GfxDevice&&) = default;
		~GfxDevice();

		void WaitForGPU();

		void ResizeBackbuffer(uint32 w, uint32 h);
		uint32 BackbufferIndex() const;
		uint32 FrameIndex() const;
		void SetBackbuffer(ID3D12GraphicsCommandList* cmd_list);
		void ClearBackbuffer();
		void SwapBuffers(bool vsync = false);

		IDXGIFactory4* GetFactory() const;
		ID3D12Device5* GetDevice() const;

		GfxCommandQueue& GetCommandQueue(GfxCommandListType type);

		ID3D12GraphicsCommandList4* GetCommandList() const;
		void ResetCommandList();
		void ExecuteCommandList();

		ID3D12RootSignature* GetCommonRootSignature() const;
		ID3D12Resource* GetBackbuffer() const;

		D3D12MA::Allocator* GetAllocator() const;
		void AddToReleaseQueue(D3D12MA::Allocation* alloc);
		void AddToReleaseQueue(ID3D12Resource* resource);

		D3D12_CPU_DESCRIPTOR_HANDLE AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE);
		void FreeOfflineDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_DESCRIPTOR_HEAP_TYPE);
		void ReserveOnlineDescriptors(size_t reserve);
		RingGPUDescriptorAllocator* GetOnlineDescriptorAllocator() const;
		LinearDynamicAllocator* GetDynamicAllocator() const;

		void GetTimestampFrequency(uint64& frequency) const;
		GPUMemoryUsage GetMemoryUsage() const
		{
			GPUMemoryUsage gpu_memory_usage{};
			D3D12MA::Budget budget;
			allocator->GetBudget(&budget, nullptr);
			gpu_memory_usage.budget = budget.BudgetBytes;
			gpu_memory_usage.usage = budget.UsageBytes;
			return gpu_memory_usage;
		}

		static constexpr uint32 BackbufferCount()
		{
			return BACKBUFFER_COUNT;
		}

		DrawIndirectSignature& GetDrawIndirectSignature() const { return *draw_indirect_signature;}
		DrawIndexedIndirectSignature& GetDrawIndexedIndirectSignature() const { return *draw_indexed_indirect_signature;}
		DispatchIndirectSignature& GetDispatchIndirectSignature() const { return *dispatch_indirect_signature;}

	private:
		uint32 width, height;
		uint32 frame_index;

		ArcPtr<IDXGIFactory4> dxgi_factory = nullptr;
		ArcPtr<ID3D12Device5> device = nullptr;

		std::unique_ptr<GfxSwapchain> swapchain;
		ReleasablePtr<D3D12MA::Allocator> allocator = nullptr;

		GfxCommandQueue graphics_queue;
		FrameResources frame_resources[BACKBUFFER_COUNT];

		GfxFence     wait_fence;
		uint64       wait_fence_value = 1;

		GfxFence     release_fence;
		uint64       release_queue_fence_value = 1;
		std::queue<ReleasableItem>  release_queue;

		ArcPtr<ID3D12RootSignature> global_root_signature = nullptr;
		std::array<std::unique_ptr<CPUDescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> offline_descriptor_allocators;

		std::unique_ptr<RingGPUDescriptorAllocator> descriptor_allocator;
		std::vector<std::unique_ptr<LinearDynamicAllocator>> dynamic_allocators;
		std::unique_ptr<LinearDynamicAllocator> dynamic_allocator_before_rendering;

		std::unique_ptr<DrawIndirectSignature> draw_indirect_signature;
		std::unique_ptr<DrawIndexedIndirectSignature> draw_indexed_indirect_signature;
		std::unique_ptr<DispatchIndirectSignature> dispatch_indirect_signature;

		struct DRED
		{
			DRED(GfxDevice* gfx);
			~DRED();

			GfxFence dred_fence;
			HANDLE   dred_wait_handle;
		};
		std::unique_ptr<DRED> dred;
		bool rendering_not_started = true;

	private:
		void SetupOptions(GfxOptions const& options, uint32& dxgi_factory_flags);
		void SetInfoQueue();
		void CreateCommonRootSignature();

		FrameResources& GetFrameResources();
		FrameResources const& GetFrameResources() const;

		void ExecuteCommandLists();
		void ProcessReleaseQueue();
	};

}