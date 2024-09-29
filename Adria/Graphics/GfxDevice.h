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

#include "GfxOptions.h"
#include "GfxFence.h"
#include "GfxCommandQueue.h"
#include "GfxCapabilities.h"
#include "GfxDescriptorAllocatorBase.h"
#include "GfxDefines.h"
#include "GfxCommandSignature.h"
#include "GfxRayTracingAS.h"
#include "GfxShadingRate.h"
#include "Utilities/Releasable.h"

namespace adria
{
	class Window;

	class GfxSwapchain;
	class GfxCommandList;
	class GfxCommandListPool;
	class GfxGraphicsCommandListPool;
	class GfxComputeCommandListPool;
	class GfxCopyCommandListPool;

	enum class GfxSubresourceType : uint8;

	class GfxTexture;
	struct GfxTextureDesc;
	struct GfxTextureDescriptorDesc;
	struct GfxTextureSubData;
	struct GfxTextureData;

	class GfxBuffer;
	struct GfxBufferDesc;
	struct GfxBufferDescriptorDesc;
	struct GfxBufferData;

	class GfxQueryHeap;
	struct GfxQueryHeapDesc;

	struct GfxGraphicsPipelineStateDesc;
	struct GfxComputePipelineStateDesc;
	struct GfxMeshShaderPipelineStateDesc;
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;
	class GfxMeshShaderPipelineState;

	class GfxLinearDynamicAllocator;
	class GfxDescriptorAllocator;
	template<bool>
	class GfxRingDescriptorAllocator;

	class GfxNsightAftermathGpuCrashTracker;
#if GFX_MULTITHREADED
	using GfxOnlineDescriptorAllocator = GfxRingDescriptorAllocator<true>;
#else
	using GfxOnlineDescriptorAllocator = GfxRingDescriptorAllocator<false>;
#endif

	struct GPUMemoryUsage
	{
		uint64 usage;
		uint64 budget;
	};

	enum class GfxVendor : uint8
	{
		AMD,
		Nvidia,
		Intel,
		Microsoft,
		Unknown
	};

	class GfxDevice
	{
		friend class GfxCommandList;
	public:
		GfxDevice(Window* window, GfxOptions const&);
		ADRIA_NONCOPYABLE(GfxDevice)
		ADRIA_DEFAULT_MOVABLE(GfxDevice)
		~GfxDevice();

		void WaitForGPU();
		
		void OnResize(uint32 w, uint32 h);
		uint32 GetBackbufferIndex() const;
		uint32 GetFrameIndex() const;

		void BeginFrame();
		void EndFrame();
		void TakePixCapture(char const* capture_name, uint32 num_frames);

		void* GetHwnd() const { return hwnd; }
		IDXGIFactory4* GetFactory() const;
		ID3D12Device5* GetDevice() const;
		ID3D12RootSignature* GetCommonRootSignature() const;
		D3D12MA::Allocator* GetAllocator() const;

		GfxCapabilities const& GetCapabilities() const { return device_capabilities; }
		GfxVendor GetVendor() const { return vendor; }
		GfxCommandQueue& GetCommandQueue(GfxCommandListType type);

		GfxCommandList* GetCommandList() const;
		GfxCommandList* GetCommandList(GfxCommandListType type) const;
		GfxCommandList* GetLatestCommandList(GfxCommandListType type) const;
		GfxCommandList* AllocateCommandList(GfxCommandListType type) const;
		void			FreeCommandList(GfxCommandList*, GfxCommandListType type);

		GfxTexture* GetBackbuffer() const;

		template<Releasable T>
		void AddToReleaseQueue(T* alloc)
		{
			release_queue.emplace(new ReleasableResource(alloc), release_queue_fence_value);
		}

		GfxDescriptor AllocateDescriptorCPU(GfxDescriptorHeapType);
		void FreeDescriptorCPU(GfxDescriptor, GfxDescriptorHeapType);

		GfxDescriptor AllocateDescriptorsGPU(uint32 count = 1);
		GfxDescriptor GetDescriptorGPU(uint32 i) const;
		void InitShaderVisibleAllocator(uint32 reserve);

		GfxLinearDynamicAllocator* GetDynamicAllocator() const;

		std::unique_ptr<GfxTexture> CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer);
		std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data);
		std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc);
		std::unique_ptr<GfxBuffer> CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& initial_data);
		std::unique_ptr<GfxBuffer> CreateBuffer(GfxBufferDesc const& desc);

		std::unique_ptr<GfxGraphicsPipelineState>		CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc);
		std::unique_ptr<GfxComputePipelineState>		CreateComputePipelineState(GfxComputePipelineStateDesc const& desc);
		std::unique_ptr<GfxMeshShaderPipelineState>	CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc);

		std::unique_ptr<GfxQueryHeap>	   CreateQueryHeap(GfxQueryHeapDesc const& desc);

		std::unique_ptr<GfxRayTracingTLAS> CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
		std::unique_ptr<GfxRayTracingBLAS> CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);

		GfxDescriptor CreateBufferSRV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr);
		GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr);
		GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr);
		GfxDescriptor CreateTextureSRV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr);
		GfxDescriptor CreateTextureUAV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr);
		GfxDescriptor CreateTextureRTV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr);
		GfxDescriptor CreateTextureDSV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr);

		void CopyDescriptors(uint32 count, GfxDescriptor dst, GfxDescriptor src, GfxDescriptorHeapType type = GfxDescriptorHeapType::CBV_SRV_UAV);
		void CopyDescriptors(GfxDescriptor dst, std::span<GfxDescriptor> src_descriptors, GfxDescriptorHeapType type = GfxDescriptorHeapType::CBV_SRV_UAV);
		void CopyDescriptors(
			std::span<std::pair<GfxDescriptor, uint32>> dst_range_starts_and_size,
			std::span<std::pair<GfxDescriptor, uint32>> src_range_starts_and_size,
			GfxDescriptorHeapType type = GfxDescriptorHeapType::CBV_SRV_UAV);

		void GetTimestampFrequency(uint64& frequency) const;
		GPUMemoryUsage GetMemoryUsage() const;

		void SetVRSInfo(GfxShadingRateInfo const& info)
		{
			shading_rate_info = info;
		}
		GfxShadingRateInfo const& GetVRSInfo() const
		{
			return shading_rate_info;
		}

		DrawIndirectSignature& GetDrawIndirectSignature() const { return *draw_indirect_signature;}
		DrawIndexedIndirectSignature& GetDrawIndexedIndirectSignature() const { return *draw_indexed_indirect_signature;}
		DispatchIndirectSignature& GetDispatchIndirectSignature() const { return *dispatch_indirect_signature;}
		DispatchMeshIndirectSignature& GetDispatchMeshIndirectSignature() const { return *dispatch_mesh_indirect_signature;}

		static constexpr uint32 GetBackbufferCount()
		{
			return GFX_BACKBUFFER_COUNT;
		}

	private:
		void* hwnd;
		uint32 width, height;
		uint32 frame_index;

		Ref<IDXGIFactory6> dxgi_factory = nullptr;
		Ref<ID3D12Device5> device = nullptr;
		GfxCapabilities device_capabilities{};
		GfxVendor vendor = GfxVendor::Unknown;

		std::unique_ptr<GfxOnlineDescriptorAllocator> gpu_descriptor_allocator;
		std::array<std::unique_ptr<GfxDescriptorAllocator>, (uint64)GfxDescriptorHeapType::Count> cpu_descriptor_allocators;

		std::unique_ptr<GfxSwapchain> swapchain;
		ReleasablePtr<D3D12MA::Allocator> allocator = nullptr;

		GfxCommandQueue graphics_queue;
		GfxCommandQueue compute_queue;
		GfxCommandQueue copy_queue;

		std::unique_ptr<GfxGraphicsCommandListPool> graphics_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		GfxFence	 frame_fence;
		uint64		 frame_fence_value = 0;
		uint64       frame_fence_values[GFX_BACKBUFFER_COUNT];

		std::unique_ptr<GfxComputeCommandListPool> compute_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		GfxFence async_compute_fence;
		uint64 async_compute_fence_value = 0;

		std::unique_ptr<GfxCopyCommandListPool> copy_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		GfxFence upload_fence;
		uint64   upload_fence_value = 0;

		GfxFence     wait_fence;
		uint64       wait_fence_value = 1;

		GfxFence     release_fence;
		uint64       release_queue_fence_value = 1;
		struct ReleasableItem
		{
			std::unique_ptr<ReleasableObject> obj;
			uint64 fence_value;

			ReleasableItem(ReleasableObject* obj, uint64 fence_value) : obj(obj), fence_value(fence_value) {}
		};
		std::queue<ReleasableItem>  release_queue;

		Ref<ID3D12RootSignature> global_root_signature = nullptr;

		std::vector<std::unique_ptr<GfxLinearDynamicAllocator>> dynamic_allocators;
		std::unique_ptr<GfxLinearDynamicAllocator> dynamic_allocator_on_init;

		std::unique_ptr<DrawIndirectSignature> draw_indirect_signature;
		std::unique_ptr<DrawIndexedIndirectSignature> draw_indexed_indirect_signature;
		std::unique_ptr<DispatchIndirectSignature> dispatch_indirect_signature;
		std::unique_ptr<DispatchMeshIndirectSignature> dispatch_mesh_indirect_signature;

		GfxShadingRateInfo shading_rate_info;

		struct DRED
		{
			DRED(GfxDevice* gfx);
			~DRED();

			GfxFence dred_fence;
			HANDLE   dred_wait_handle;
		};
		std::unique_ptr<DRED> dred;
		bool rendering_not_started = true;
		bool pix_dll_loaded = false;

		std::unique_ptr<GfxNsightAftermathGpuCrashTracker> nsight_aftermath;

	private:
		void SetupOptions(GfxOptions const& options, uint32& dxgi_factory_flags);
		void SetInfoQueue();
		void CreateCommonRootSignature();

		void ProcessReleaseQueue();
		GfxOnlineDescriptorAllocator* GetDescriptorAllocator() const;

		GfxDescriptor CreateBufferView(GfxBuffer const* buffer, GfxSubresourceType view_type, GfxBufferDescriptorDesc const& view_desc, GfxBuffer const* uav_counter = nullptr);
		GfxDescriptor CreateTextureView(GfxTexture const* texture, GfxSubresourceType view_type, GfxTextureDescriptorDesc const& view_desc);

	};

}