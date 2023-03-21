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
#include "GfxDescriptorAllocatorBase.h"
#include "GfxDefines.h"
#include "CommandSignature.h"
#include "../Utilities/Releasable.h"

namespace adria
{
	class GfxSwapchain;
	class GfxCommandList;
	class GfxTexture;
	class GfxTextureSubresourceDesc;
	class GfxBuffer;
	class GfxBufferSubresourceDesc;

	class GfxDescriptorAllocator;
	template<bool>
	class GfxRingDescriptorAllocator;
	using GfxMTRingDescriptorAllocator = GfxRingDescriptorAllocator<GFX_MULTITHREADED>;

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
		struct ReleasableItem
		{
			std::unique_ptr<ReleasableObject> obj;
			uint64 fence_value;

			ReleasableItem(ReleasableObject* obj, size_t fence_value) : obj(obj), fence_value(fence_value) {}
		};

	public:
		explicit GfxDevice(GfxOptions const&);
		GfxDevice(GfxDevice const&) = delete;
		GfxDevice(GfxDevice&&);
		~GfxDevice();

		void WaitForGPU();

		void OnResize(uint32 w, uint32 h);
		uint32 BackbufferIndex() const;
		uint32 FrameIndex() const;

		void BeginFrame();
		void EndFrame(bool vsync = false);

		IDXGIFactory4* GetFactory() const;
		ID3D12Device5* GetDevice() const;
		ID3D12RootSignature* GetCommonRootSignature() const;

		GfxCommandQueue& GetCommandQueue(GfxCommandListType type);
		GfxCommandList* GetCommandList(GfxCommandListType type) const;
		GfxTexture* GetBackbuffer() const;

		GFX_DEPRECATED ID3D12GraphicsCommandList4* GetCommandList() const;

		D3D12MA::Allocator* GetAllocator() const;
		template<Releasable T>
		void AddToReleaseQueue(T* alloc)
		{
			release_queue.emplace(new ReleasableResource(alloc), release_queue_fence_value);
		}

		GfxDescriptor AllocateOfflineDescriptor(GfxDescriptorHeapType);
		void FreeOfflineDescriptor(GfxDescriptor, GfxDescriptorHeapType);
		void InitShaderVisibleAllocator(size_t reserve);

		GfxDescriptor CreateBufferSRV(GfxBuffer const*, GfxBufferSubresourceDesc const* = nullptr);
		GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBufferSubresourceDesc const* = nullptr);
		GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBuffer const*, GfxBufferSubresourceDesc const* = nullptr);
		GfxDescriptor CreateTextureSRV(GfxTexture const*, GfxTextureSubresourceDesc const* = nullptr);
		GfxDescriptor CreateTextureUAV(GfxTexture const*, GfxTextureSubresourceDesc const* = nullptr);
		GfxDescriptor CreateTextureRTV(GfxTexture const*, GfxTextureSubresourceDesc const* = nullptr);
		GfxDescriptor CreateTextureDSV(GfxTexture const*, GfxTextureSubresourceDesc const* = nullptr);

		//#todo : copy descriptors API

		GfxMTRingDescriptorAllocator* GetDescriptorAllocator() const;
		GfxLinearDynamicAllocator* GetDynamicAllocator() const;

		void GetTimestampFrequency(uint64& frequency) const;
		GPUMemoryUsage GetMemoryUsage() const;

		DrawIndirectSignature& GetDrawIndirectSignature() const { return *draw_indirect_signature;}
		DrawIndexedIndirectSignature& GetDrawIndexedIndirectSignature() const { return *draw_indexed_indirect_signature;}
		DispatchIndirectSignature& GetDispatchIndirectSignature() const { return *dispatch_indirect_signature;}

		static constexpr uint32 BackbufferCount()
		{
			return GFX_BACKBUFFER_COUNT;
		}
	private:
		uint32 width, height;
		uint32 frame_index;

		ArcPtr<IDXGIFactory4> dxgi_factory = nullptr;
		ArcPtr<ID3D12Device5> device = nullptr;

		std::unique_ptr<GfxMTRingDescriptorAllocator> gpu_descriptor_allocator;
		std::array<std::unique_ptr<GfxDescriptorAllocator>, (size_t)GfxDescriptorHeapType::Count> cpu_descriptor_allocators;

		std::unique_ptr<GfxSwapchain> swapchain;
		ReleasablePtr<D3D12MA::Allocator> allocator = nullptr;

		GfxCommandQueue graphics_queue;
		GfxCommandQueue compute_queue;
		GfxCommandQueue copy_queue;

		std::unique_ptr<GfxCommandList> graphics_cmd_lists[GFX_BACKBUFFER_COUNT];
		GfxFence	 frame_fence;
		uint64		 frame_fence_value = 0;
		uint64       frame_fence_values[GFX_BACKBUFFER_COUNT];

		std::unique_ptr<GfxCommandList> compute_cmd_lists[GFX_BACKBUFFER_COUNT];
		GfxFence async_compute_fence;
		uint64 async_compute_fence_value = 0;

		std::unique_ptr<GfxCommandList> upload_cmd_lists[GFX_BACKBUFFER_COUNT];
		GfxFence upload_fence;
		uint64 upload_fence_value = 0;

		GfxFence     wait_fence;
		uint64       wait_fence_value = 1;

		GfxFence     release_fence;
		uint64       release_queue_fence_value = 1;
		std::queue<ReleasableItem>  release_queue;

		ArcPtr<ID3D12RootSignature> global_root_signature = nullptr;

		std::vector<std::unique_ptr<GfxLinearDynamicAllocator>> dynamic_allocators;
		std::unique_ptr<GfxLinearDynamicAllocator> dynamic_allocator_before_rendering;

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
		bool rendering_not_started = true; //#todo : remove

	private:
		void SetupOptions(GfxOptions const& options, uint32& dxgi_factory_flags);
		void SetInfoQueue();
		void CreateCommonRootSignature();

		void ProcessReleaseQueue();

		GfxDescriptor CreateBufferView(GfxBuffer const* buffer, GfxSubresourceType view_type, GfxBufferSubresourceDesc const& view_desc, GfxBuffer const* uav_counter = nullptr);
		GfxDescriptor CreateTextureView(GfxTexture const* texture, GfxSubresourceType view_type, GfxTextureSubresourceDesc const& view_desc);

	};

}