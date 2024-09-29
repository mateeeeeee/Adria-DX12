#include <map>
#include <dxgidebug.h>
#include "pix3.h"
#include "GfxDevice.h"
#include "GfxSwapchain.h"
#include "GfxCommandList.h"
#include "GfxCommandListPool.h"
#include "GfxTexture.h"
#include "GfxBuffer.h"
#include "GfxDescriptorAllocator.h"
#include "GfxRingDescriptorAllocator.h"
#include "GfxLinearDynamicAllocator.h"
#include "GfxQueryHeap.h"
#include "GfxPipelineState.h"
#include "GfxNsightAftermathGpuCrashTracker.h"
#include "d3dx12.h"
#include "Logging/Logger.h"
#include "Core/Window.h"
#include "Core/ConsoleManager.h"


extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern LPCSTR D3D12SDKPath = ".\\D3D12\\"; }
extern "C" { __declspec(dllexport) extern UINT NvOptimusEnablement = true; }

namespace adria
{
	namespace
	{
		inline constexpr wchar_t const* DredBreadcrumbOpName(D3D12_AUTO_BREADCRUMB_OP op)
		{
			switch (op)
			{
			case D3D12_AUTO_BREADCRUMB_OP_SETMARKER: return L"Set marker";
			case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT: return L"Begin event";
			case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT: return L"End event";
			case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED: return L"Draw instanced";
			case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED: return L"Draw indexed instanced";
			case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT: return L"Execute indirect";
			case D3D12_AUTO_BREADCRUMB_OP_DISPATCH: return L"Dispatch";
			case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION: return L"Copy buffer region";
			case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION: return L"Copy texture region";
			case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE: return L"Copy resource";
			case D3D12_AUTO_BREADCRUMB_OP_COPYTILES: return L"Copy tiles";
			case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE: return L"Resolve subresource";
			case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW: return L"Clear render target view";
			case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW: return L"Clear unordered access view";
			case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW: return L"Clear depth stencil view";
			case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER: return L"Resource barrier";
			case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE: return L"Execute bundle";
			case D3D12_AUTO_BREADCRUMB_OP_PRESENT: return L"Present";
			case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA: return L"Resolve query data";
			case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION: return L"Begin submission";
			case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION: return L"End submission";
			case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME: return L"Decode frame";
			case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES: return L"Process frames";
			case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT: return L"Atomic copy buffer uint";
			case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64: return L"Atomic copy buffer uint64";
			case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION: return L"Resolve subresource region";
			case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE: return L"Write buffer immediate";
			case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1: return L"Decode frame 1";
			case D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION: return L"Set protected resource session";
			case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2: return L"Decode frame 2";
			case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1: return L"Process frames 1";
			case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE: return L"Build raytracing acceleration structure";
			case D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO: return L"Emit raytracing acceleration structure post build info";
			case D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE: return L"Copy raytracing acceleration structure";
			case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS: return L"Dispatch rays";
			case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND: return L"Initialize meta command";
			case D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND: return L"Execute meta command";
			case D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION: return L"Estimate motion";
			case D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP: return L"Resolve motion vector heap";
			case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1: return L"Set pipeline state 1";
			case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND: return L"Initialize extension command";
			case D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND: return L"Execute extension command";
			case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH: return L"Dispatch mesh";
			default: return L"Unknown";
			}
		}
		inline constexpr wchar_t const* DredAllocationName(D3D12_DRED_ALLOCATION_TYPE type)
		{
			switch (type)
			{
			case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE: return L"Command queue";
			case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR: return L"Command allocator";
			case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE: return L"Pipeline state";
			case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST: return L"Command list";
			case D3D12_DRED_ALLOCATION_TYPE_FENCE: return L"Fence";
			case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP: return L"Descriptor heap";
			case D3D12_DRED_ALLOCATION_TYPE_HEAP: return L"Heap";
			case D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP: return L"Query heap";
			case D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE: return L"Command signature";
			case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY: return L"Pipeline library";
			case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER: return L"Video decoder";
			case D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR: return L"Video processor";
			case D3D12_DRED_ALLOCATION_TYPE_RESOURCE: return L"Resource";
			case D3D12_DRED_ALLOCATION_TYPE_PASS: return L"Pass";
			case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION: return L"Crypto session";
			case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY: return L"Crypto session policy";
			case D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION: return L"Protected resource session";
			case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP: return L"Video decoder heap";
			case D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL: return L"Command pool";
			case D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER: return L"Command recorder";
			case D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT: return L"State object";
			case D3D12_DRED_ALLOCATION_TYPE_METACOMMAND: return L"Meta command";
			case D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP: return L"Scheduling group";
			case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR: return L"Video motion estimator";
			case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP: return L"Video motion vector heap";
			case D3D12_DRED_ALLOCATION_TYPE_INVALID: return L"Invalid";
			default: return L"Unknown";
			}
		}
		void LogDredInfo(ID3D12Device5* device, ID3D12DeviceRemovedExtendedData1* dred)
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 DredAutoBreadcrumbsOutput;
			if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&DredAutoBreadcrumbsOutput)))
			{
				ADRIA_LOG(DEBUG, "[DRED] Last tracked GPU operations:");
				std::map<int32, wchar_t const*> contextStrings;
				D3D12_AUTO_BREADCRUMB_NODE1 const* pNode = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
				while (pNode && pNode->pLastBreadcrumbValue)
				{
					int32 lastCompletedOp = *pNode->pLastBreadcrumbValue;
					if (lastCompletedOp != (int)pNode->BreadcrumbCount && lastCompletedOp != 0)
					{
						char const* cmd_list_name = "cmd_list";
						char const* queue_name = "graphics queue";
						ADRIA_LOG(DEBUG, "[DRED] Commandlist \"%s\" on CommandQueue \"%s\", %d completed of %d", cmd_list_name, queue_name, lastCompletedOp, pNode->BreadcrumbCount);

						int32 firstOp = std::max<int32>(lastCompletedOp - 100, 0);
						int32 lastOp = std::min<int32>(lastCompletedOp + 20, int32(pNode->BreadcrumbCount) - 1);

						contextStrings.clear();
						for (uint32 breadcrumbContext = firstOp; breadcrumbContext < pNode->BreadcrumbContextsCount; ++breadcrumbContext)
						{
							const D3D12_DRED_BREADCRUMB_CONTEXT& context = pNode->pBreadcrumbContexts[breadcrumbContext];
							contextStrings[context.BreadcrumbIndex] = context.pContextString;
						}

						for (int32 op = firstOp; op <= lastOp; ++op)
						{
							D3D12_AUTO_BREADCRUMB_OP breadcrumbOp = pNode->pCommandHistory[op];

							std::wstring context_string;
							auto it = contextStrings.find(op);
							if (it != contextStrings.end())
							{
								context_string = it->second;
							}

							wchar_t const* opName = DredBreadcrumbOpName(breadcrumbOp);
							ADRIA_LOG(DEBUG, "\tOp: %d, %ls%ls%s", op, opName, context_string.c_str(), (op + 1 == lastCompletedOp) ? " - Last completed" : "");
						}
					}
					pNode = pNode->pNext;
				}
			}

			D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
			if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&DredPageFaultOutput)))
			{
				ADRIA_LOG(DEBUG, "[DRED] PageFault at VA GPUAddress \"0x%llx\"", DredPageFaultOutput.PageFaultVA);

				D3D12_DRED_ALLOCATION_NODE const* pNode = DredPageFaultOutput.pHeadExistingAllocationNode;
				if (pNode)
				{
					ADRIA_LOG(DEBUG, "[DRED] Active objects with VA ranges that match the faulting VA:");
					while (pNode)
					{
						wchar_t const* AllocTypeName = DredAllocationName(pNode->AllocationType);
						ADRIA_LOG(DEBUG, "\tName: %s (Type: %ls)", pNode->ObjectNameA, AllocTypeName);
						pNode = pNode->pNext;
					}
				}

				pNode = DredPageFaultOutput.pHeadRecentFreedAllocationNode;
				if (pNode)
				{
					ADRIA_LOG(DEBUG, "[DRED] Recent freed objects with VA ranges that match the faulting VA:");
					while (pNode)
					{
						uint32 allocTypeIndex = pNode->AllocationType - D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE;
						wchar_t const* AllocTypeName = DredAllocationName(pNode->AllocationType);
						ADRIA_LOG(DEBUG, "\tName: %s (Type: %ls)", pNode->ObjectNameA, AllocTypeName);
						pNode = pNode->pNext;
					}
				}
			}
		}
		void DeviceRemovedHandler(void* _device, BYTE)
		{
			ID3D12Device5* device = static_cast<ID3D12Device5*>(_device);
			HRESULT removed_reason = device->GetDeviceRemovedReason();
			ADRIA_LOG(ERROR, "Device removed, reason code: %ld", removed_reason);

			Ref<ID3D12DeviceRemovedExtendedData1> dred;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(dred.GetAddressOf())))) ADRIA_LOG(ERROR, "Failed to get DRED interface");
			else LogDredInfo(device, dred.Get());
			std::exit(1);
		}
		inline void ReportLiveObjects()
		{
			Ref<IDXGIDebug1> dxgi_debug;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgi_debug.GetAddressOf()))))
			{
				dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
			}
		}

		enum GfxVendorId : uint32
		{
			GfxVendorId_AMD = 0x1002,
			GfxVendorId_Nvidia = 0x10de,
			GfxVendorId_Intel = 0x8086,
			GfxVendorId_Microsoft = 0x1414
		};
		inline GfxVendor GetGfxVendor(uint32 vendor_id)
		{
			switch (vendor_id)
			{
			case GfxVendorId_AMD: return GfxVendor::AMD;
			case GfxVendorId_Nvidia: return GfxVendor::Nvidia;
			case GfxVendorId_Intel: return GfxVendor::Intel;
			case GfxVendorId_Microsoft: return GfxVendor::Microsoft;
			}
			return GfxVendor::Unknown;
		}
		inline char const* GetGfxVendorName(GfxVendor gfx_vendor)
		{
			switch (gfx_vendor)
			{
			case GfxVendor::AMD: return "AMD";
			case GfxVendor::Nvidia: return "Nvidia";
			case GfxVendor::Intel: return "Intel";
			case GfxVendor::Microsoft: return "Microsoft";
			}
			return "Unknown";
		}
	}

	static TAutoConsoleVariable<bool> VSync("rhi.VSync", false, "0: VSync is disabled. 1: VSync is enabled.");

	GfxDevice::DRED::DRED(GfxDevice* gfx)
	{
		dred_fence.Create(gfx, "DRED Fence");
		dred_wait_handle = CreateEventA(nullptr, false, false, nullptr);
		if (!dred_wait_handle) return;
		static_cast<ID3D12Fence*>(dred_fence)->SetEventOnCompletion(UINT64_MAX, dred_wait_handle);
		ADRIA_ASSERT(RegisterWaitForSingleObject(&dred_wait_handle, dred_wait_handle, DeviceRemovedHandler, gfx->GetDevice(), INFINITE, 0));
	}
	GfxDevice::DRED::~DRED()
	{
		dred_fence.Signal(UINT64_MAX);
		ADRIA_ASSERT(UnregisterWaitEx(dred_wait_handle, INVALID_HANDLE_VALUE));
		CloseHandle(dred_wait_handle);
	}

	GfxDevice::GfxDevice(Window* window, GfxOptions const& options)
		: frame_index(0), shading_rate_info{}
	{
		VSync->Set(options.vsync);
		hwnd = window->Handle();
		width = window->Width();
		height = window->Height();

		HRESULT hr = E_FAIL;
		uint32 dxgi_factory_flags = 0;
		SetupOptions(options, dxgi_factory_flags);
		GFX_CHECK_HR(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(dxgi_factory.GetAddressOf())));

		Ref<IDXGIAdapter4> adapter;
		uint32 adapter_index = 0;
		ADRIA_LOG(INFO, "Available adapters:");
		DXGI_GPU_PREFERENCE gpu_preference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
		while (dxgi_factory->EnumAdapterByGpuPreference(adapter_index++, gpu_preference, IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())) == S_OK)
		{
			DXGI_ADAPTER_DESC3 desc{};
			adapter->GetDesc3(&desc);
			std::wstring adapter_wide_description(desc.Description);
			std::string adapter_description = ToString(adapter_wide_description);
			ADRIA_LOG(INFO, "\t%s - %f GB", adapter_description.c_str(), (float)desc.DedicatedVideoMemory / (1 << 30) );
		}
		dxgi_factory->EnumAdapterByGpuPreference(0, gpu_preference, IID_PPV_ARGS(adapter.GetAddressOf()));
		DXGI_ADAPTER_DESC3 desc{};
		adapter->GetDesc3(&desc);

		vendor = GetGfxVendor(desc.VendorId);
		ADRIA_ASSERT(vendor != GfxVendor::Unknown);
		char const* vendor_name = GetGfxVendorName(vendor);
		ADRIA_LOG(INFO, "Vendor: %s", vendor_name);

		std::wstring adapter_wide_description(desc.Description);
		std::string adapter_description = ToString(adapter_wide_description);
		ADRIA_LOG(INFO, "GPU: %s", adapter_description.c_str());

		
		D3D_FEATURE_LEVEL feature_levels[] =
		{
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};

		if (options.aftermath) nsight_aftermath = std::make_unique<GfxNsightAftermathGpuCrashTracker>(this);

		GFX_CHECK_HR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf())));
		D3D12_FEATURE_DATA_FEATURE_LEVELS caps{};
		caps.pFeatureLevelsRequested = feature_levels;
		caps.NumFeatureLevels = ARRAYSIZE(feature_levels);
		GFX_CHECK_HR(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &caps, sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS)));
		GFX_CHECK_HR(D3D12CreateDevice(adapter.Get(), caps.MaxSupportedFeatureLevel, IID_PPV_ARGS(device.ReleaseAndGetAddressOf())));

		if (!device_capabilities.Initialize(this))
		{
			ADRIA_DEBUGBREAK();
			std::exit(EXIT_FAILURE);
		}
		if(nsight_aftermath) nsight_aftermath->Initialize();

		D3D12MA::ALLOCATOR_DESC allocator_desc{};
		allocator_desc.pDevice = device.Get();
		allocator_desc.pAdapter = adapter.Get();
		D3D12MA::Allocator* _allocator = nullptr;
		GFX_CHECK_HR(D3D12MA::CreateAllocator(&allocator_desc, &_allocator));
		allocator.reset(_allocator);

		graphics_queue.Create(this, GfxCommandListType::Graphics, "Graphics Queue");
		compute_queue.Create(this, GfxCommandListType::Compute, "Compute Queue");
		copy_queue.Create(this, GfxCommandListType::Copy, "Copy Queue");

		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			graphics_cmd_list_pool[i] = std::make_unique<GfxGraphicsCommandListPool>(this);
			compute_cmd_list_pool[i]  = std::make_unique<GfxComputeCommandListPool>(this);
			copy_cmd_list_pool[i]	  = std::make_unique<GfxCopyCommandListPool>(this);
		}

		for (uint32 i = 0; i < (uint32)GfxDescriptorHeapType::Count; ++i)
		{
			GfxDescriptorAllocatorDesc desc{};
			desc.descriptor_count = 1024;
			desc.shader_visible = false;
			desc.type = static_cast<GfxDescriptorHeapType>(i);
			cpu_descriptor_allocators[i] = std::make_unique<GfxDescriptorAllocator>(this, desc);
		}
		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i) dynamic_allocators.emplace_back(new GfxLinearDynamicAllocator(this, 1 << 20));
		dynamic_allocator_on_init.reset(new GfxLinearDynamicAllocator(this, 1 << 30));

		GfxSwapchainDesc swapchain_desc{};
		swapchain_desc.width = width;
		swapchain_desc.height = height;
		swapchain_desc.fullscreen_windowed = true;
		swapchain_desc.backbuffer_format = GfxFormat::R8G8B8A8_UNORM;
		swapchain = std::make_unique<GfxSwapchain>(this, swapchain_desc);

		frame_fence.Create(this, "Frame Fence");
		upload_fence.Create(this, "Upload Fence");
		async_compute_fence.Create(this, "Async Compute Fence");
		wait_fence.Create(this, "Wait Fence");
		release_fence.Create(this, "Release Fence");

		draw_indirect_signature = std::make_unique<DrawIndirectSignature>(device.Get());
		draw_indexed_indirect_signature = std::make_unique<DrawIndexedIndirectSignature>(device.Get());
		dispatch_indirect_signature = std::make_unique<DispatchIndirectSignature>(device.Get());
		if(device_capabilities.SupportsMeshShaders()) dispatch_mesh_indirect_signature = std::make_unique<DispatchMeshIndirectSignature>(device.Get());

		SetInfoQueue();
		CreateCommonRootSignature();

		std::atexit(ReportLiveObjects);
		if (options.dred) dred = std::make_unique<DRED>(this);
	}
	GfxDevice::~GfxDevice()
	{
		WaitForGPU();
		ProcessReleaseQueue();
		frame_fence.Wait(frame_fence_values[swapchain->GetBackbufferIndex()]);
	}

	void GfxDevice::WaitForGPU()
	{
		graphics_queue.Signal(wait_fence, wait_fence_value);
		copy_queue.Signal(wait_fence, wait_fence_value);
		wait_fence.Wait(wait_fence_value);
		wait_fence.Wait(wait_fence_value);
		wait_fence_value++;
	}

	void GfxDevice::OnResize(uint32 w, uint32 h)
	{
		if ((width != w || height != h) && width > 0 && height > 0)
		{
			width = w;
			height = h;
			WaitForGPU();
			for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i) frame_fence_values[i] = frame_fence_values[swapchain->GetBackbufferIndex()];
			swapchain->OnResize(w, h);
		}
	}
	uint32 GfxDevice::GetBackbufferIndex() const
	{
		return swapchain->GetBackbufferIndex();
	}
	uint32 GfxDevice::GetFrameIndex() const { return frame_index; }

	void GfxDevice::BeginFrame()
	{
		if (rendering_not_started) [[unlikely]]
		{
			rendering_not_started = false;
			dynamic_allocator_on_init.reset();
		}

		uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		gpu_descriptor_allocator->ReleaseCompletedFrames(frame_index);
		dynamic_allocators[backbuffer_index]->Clear();

		graphics_cmd_list_pool[backbuffer_index]->BeginCmdLists();
		copy_cmd_list_pool[backbuffer_index]->BeginCmdLists();
	}
	void GfxDevice::EndFrame()
	{
		uint32 backbuffer_index = swapchain->GetBackbufferIndex();

		graphics_cmd_list_pool[backbuffer_index]->EndCmdLists();
		copy_cmd_list_pool[backbuffer_index]->EndCmdLists();

		graphics_queue.ExecuteCommandListPool(*graphics_cmd_list_pool[backbuffer_index]);
		copy_queue.ExecuteCommandListPool(*copy_cmd_list_pool[backbuffer_index]);
		ProcessReleaseQueue();

		bool present_successful = swapchain->Present(VSync.Get());
		if (!present_successful && nsight_aftermath && nsight_aftermath->IsInitialized())
		{
			nsight_aftermath->HandleGpuCrash();
			MessageBoxA(nullptr, "Swapchain present failed!", "GPU Crash", MB_OK);
			std::exit(1);
		}

		backbuffer_index = swapchain->GetBackbufferIndex();
		frame_fence_values[backbuffer_index] = frame_fence_value;
		graphics_queue.Signal(frame_fence, frame_fence_value);
		++frame_fence_value;

		backbuffer_index = swapchain->GetBackbufferIndex();
		frame_fence.Wait(frame_fence_values[backbuffer_index]);

		++frame_index;
		gpu_descriptor_allocator->FinishCurrentFrame(frame_index);
	}

	void GfxDevice::TakePixCapture(char const* capture_name, uint32 num_frames)
	{
		ADRIA_ASSERT(num_frames != 0);
		if (!pix_dll_loaded)
		{
			ADRIA_LOG(WARNING, "All PIX capture requests are ignored because PIX dll wasn't loaded! Did you pass -pix as a command line argument?");
			return;
		}

		std::string full_capture_name = std::string(capture_name) + "_" + std::to_string(frame_index) + ".wpix";
		std::wstring wcapture_name = ToWideString(full_capture_name);
		GFX_CHECK_HR(PIXGpuCaptureNextFrames(wcapture_name.c_str(), num_frames));
		ADRIA_LOG(INFO, "Saving capture of %d frame(s) to %s...", num_frames, full_capture_name.c_str());
	}

	IDXGIFactory4* GfxDevice::GetFactory() const
	{
		return dxgi_factory.Get();
	}
	ID3D12Device5* GfxDevice::GetDevice() const
	{
		return device.Get();
	}
	ID3D12RootSignature* GfxDevice::GetCommonRootSignature() const
	{
		return global_root_signature.Get();
	}

	D3D12MA::Allocator* GfxDevice::GetAllocator() const
	{
		return allocator.get();
	}

	GfxTexture* GfxDevice::GetBackbuffer() const
	{
		return swapchain->GetBackbuffer();
	}
	GfxCommandQueue& GfxDevice::GetCommandQueue(GfxCommandListType type)
	{
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_queue;
		case GfxCommandListType::Compute:
			return compute_queue;
		case GfxCommandListType::Copy:
			return copy_queue;
		default:
			return graphics_queue;
		}
		ADRIA_UNREACHABLE();
	}

	GfxCommandList* GfxDevice::GetCommandList(GfxCommandListType type) const
	{
		uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		default:
			return graphics_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		}
		ADRIA_UNREACHABLE();
	}
	GfxCommandList* GfxDevice::GetCommandList() const
	{
		return GetCommandList(GfxCommandListType::Graphics);
	}
	GfxCommandList* GfxDevice::GetLatestCommandList(GfxCommandListType type) const
	{
		uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		default:
			return graphics_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		}
		ADRIA_UNREACHABLE();
	}
	GfxCommandList* GfxDevice::AllocateCommandList(GfxCommandListType type) const
	{
		uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		default:
			return graphics_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		}
		ADRIA_UNREACHABLE();
	}
	void GfxDevice::FreeCommandList(GfxCommandList* cmd_list, GfxCommandListType type)
	{
		uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		default:
			return graphics_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		}
		ADRIA_UNREACHABLE();
	}

	void GfxDevice::CopyDescriptors(uint32 count, GfxDescriptor dst, GfxDescriptor src, GfxDescriptorHeapType type /*= GfxDescriptorHeapType::CBV_SRV_UAV*/)
	{
		device->CopyDescriptorsSimple(count, dst, src, ToD3D12HeapType(type));
	}
	void GfxDevice::CopyDescriptors(GfxDescriptor dst, std::span<GfxDescriptor> src_descriptors, GfxDescriptorHeapType type /*= GfxDescriptorHeapType::CBV_SRV_UAV*/)
	{
		uint32 const dst_ranges_count = 1;
		uint32 const src_ranges_count = (uint32)src_descriptors.size();

		D3D12_CPU_DESCRIPTOR_HANDLE dst_handles[] = { dst };
		uint32 dst_range_sizes[] = { (uint32)src_descriptors.size() };

		std::vector<uint32> src_range_sizes(src_descriptors.size(), 1);
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_handles(src_descriptors.size());
		for (uint64 i = 0; i < src_handles.size(); ++i) src_handles[i] = src_descriptors[i];

		device->CopyDescriptors(dst_ranges_count, dst_handles, dst_range_sizes,
			src_ranges_count, src_handles.data(), src_range_sizes.data(), ToD3D12HeapType(type));
	}
	void GfxDevice::CopyDescriptors(std::span<std::pair<GfxDescriptor, uint32>> dst_range_starts_and_size, std::span<std::pair<GfxDescriptor, uint32>> src_range_starts_and_size, GfxDescriptorHeapType type /*= GfxDescriptorHeapType::CBV_SRV_UAV*/)
	{
		uint32 const dst_ranges_count = (uint32)dst_range_starts_and_size.size();
		uint32 const src_ranges_count = (uint32)src_range_starts_and_size.size();
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dst_handles(dst_ranges_count);
		std::vector<uint32> dst_range_sizes(dst_ranges_count);
		for (uint64 i = 0; i < dst_ranges_count; ++i)
		{
			dst_handles[i] = dst_range_starts_and_size[i].first;
			dst_range_sizes[i] = dst_range_starts_and_size[i].second;
		}

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_handles(src_ranges_count);
		std::vector<uint32> src_range_sizes(src_ranges_count);
		for (uint64 i = 0; i < src_ranges_count; ++i)
		{
			src_handles[i] = src_range_starts_and_size[i].first;
			src_range_sizes[i] = src_range_starts_and_size[i].second;
		}

		device->CopyDescriptors(dst_ranges_count, dst_handles.data(), dst_range_sizes.data(),
			src_ranges_count, src_handles.data(), src_range_sizes.data(), ToD3D12HeapType(type));
	}

	GfxDescriptor GfxDevice::AllocateDescriptorCPU(GfxDescriptorHeapType type)
	{
		return cpu_descriptor_allocators[(uint64)type]->AllocateDescriptor();
	}
	void GfxDevice::FreeDescriptorCPU(GfxDescriptor descriptor, GfxDescriptorHeapType type)
	{
		cpu_descriptor_allocators[(uint64)type]->FreeDescriptor(descriptor);
	}

	GfxDescriptor GfxDevice::AllocateDescriptorsGPU(uint32 count)
	{
		return GetDescriptorAllocator()->Allocate(count);
	}
	GfxDescriptor GfxDevice::GetDescriptorGPU(uint32 i) const
	{
		return GetDescriptorAllocator()->GetHandle(i);
	}

	GfxOnlineDescriptorAllocator* GfxDevice::GetDescriptorAllocator() const
	{
		return gpu_descriptor_allocator.get();
	}
	GfxLinearDynamicAllocator* GfxDevice::GetDynamicAllocator() const
	{
		if (rendering_not_started) return dynamic_allocator_on_init.get();
		else return dynamic_allocators[swapchain->GetBackbufferIndex()].get();
	}

	void GfxDevice::InitShaderVisibleAllocator(uint32 reserve)
	{
		gpu_descriptor_allocator = std::make_unique<GfxOnlineDescriptorAllocator>(this, 32767, reserve);
	}

	std::unique_ptr<GfxTexture> GfxDevice::CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer)
	{
		return std::make_unique<GfxTexture>(this, desc, backbuffer);
	}

	std::unique_ptr<adria::GfxTexture> GfxDevice::CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data /*= {}*/)
	{
		return std::make_unique<GfxTexture>(this, desc, data);
	}

	std::unique_ptr<adria::GfxTexture> GfxDevice::CreateTexture(GfxTextureDesc const& desc)
	{
		return std::make_unique<GfxTexture>(this, desc, GfxTextureData{});
	}

	std::unique_ptr<GfxBuffer>  GfxDevice::CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& initial_data)
	{
		return std::make_unique<GfxBuffer>(this, desc, initial_data);
	}

	std::unique_ptr<GfxBuffer> GfxDevice::CreateBuffer(GfxBufferDesc const& desc)
	{
		return std::make_unique<GfxBuffer>(this, desc);
	}

	std::unique_ptr<GfxGraphicsPipelineState> GfxDevice::CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc)
	{
		return std::make_unique<GfxGraphicsPipelineState>(this, desc);
	}
	std::unique_ptr<GfxComputePipelineState> GfxDevice::CreateComputePipelineState(GfxComputePipelineStateDesc const& desc)
	{
		return std::make_unique<GfxComputePipelineState>(this, desc);
	}
	std::unique_ptr<GfxMeshShaderPipelineState> GfxDevice::CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc)
	{
		return std::make_unique<GfxMeshShaderPipelineState>(this, desc);
	}

	std::unique_ptr<GfxQueryHeap> GfxDevice::CreateQueryHeap(GfxQueryHeapDesc const& desc)
	{
		return std::make_unique<GfxQueryHeap>(this, desc);
	}

	std::unique_ptr<GfxRayTracingTLAS> GfxDevice::CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags)
	{
		return std::make_unique<GfxRayTracingTLAS>(this, instances, flags);
	}
	std::unique_ptr<GfxRayTracingBLAS> GfxDevice::CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags)
	{
		return std::make_unique<GfxRayTracingBLAS>(this, geometries, flags);
	}

	GfxDescriptor GfxDevice::CreateBufferSRV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
	{
		GfxBufferDescriptorDesc _desc = desc ? *desc : GfxBufferDescriptorDesc{};
		return CreateBufferView(buffer, GfxSubresourceType::SRV, _desc);
	}
	GfxDescriptor GfxDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
	{
		GfxBufferDescriptorDesc _desc = desc ? *desc : GfxBufferDescriptorDesc{};
		return CreateBufferView(buffer, GfxSubresourceType::UAV, _desc);
	}
	GfxDescriptor GfxDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBuffer const* counter, GfxBufferDescriptorDesc const* desc/*= nullptr*/)
	{
		GfxBufferDescriptorDesc _desc = desc ? *desc : GfxBufferDescriptorDesc{};
		return CreateBufferView(buffer, GfxSubresourceType::UAV, _desc, counter);
	}
	GfxDescriptor GfxDevice::CreateTextureSRV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return CreateTextureView(texture, GfxSubresourceType::SRV, _desc);
	}

	GfxDescriptor GfxDevice::CreateTextureUAV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return CreateTextureView(texture, GfxSubresourceType::UAV, _desc);
	}
	GfxDescriptor GfxDevice::CreateTextureRTV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return CreateTextureView(texture, GfxSubresourceType::RTV, _desc);
	}
	GfxDescriptor GfxDevice::CreateTextureDSV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return CreateTextureView(texture, GfxSubresourceType::DSV, _desc);
	}

	void GfxDevice::GetTimestampFrequency(uint64& frequency) const
	{
		frequency = graphics_queue.GetTimestampFrequency();
	}
	GPUMemoryUsage GfxDevice::GetMemoryUsage() const
	{
		GPUMemoryUsage gpu_memory_usage{};
		D3D12MA::Budget budget;
		allocator->GetBudget(&budget, nullptr);
		gpu_memory_usage.budget = budget.BudgetBytes;
		gpu_memory_usage.usage = budget.UsageBytes;
		return gpu_memory_usage;
	}

	void GfxDevice::ProcessReleaseQueue()
	{
		while (!release_queue.empty())
		{
			if (!release_fence.IsCompleted(release_queue.front().fence_value)) break;
			release_queue.pop();
		}
		graphics_queue.Signal(release_fence, release_queue_fence_value);
		++release_queue_fence_value;
	}
	void GfxDevice::SetInfoQueue()
	{
		Ref<ID3D12InfoQueue> info_queue;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(info_queue.GetAddressOf()))))
		{
			//D3D12_MESSAGE_CATEGORY Categories[0] = {};
			D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			D3D12_MESSAGE_ID DenyIds[] =
			{
				D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
				D3D12_MESSAGE_ID_COMMAND_ALLOCATOR_SYNC
			};

			D3D12_INFO_QUEUE_FILTER NewFilter{};
			NewFilter.DenyList.NumCategories = 0;
			NewFilter.DenyList.pCategoryList = NULL;
			NewFilter.DenyList.NumSeverities = ARRAYSIZE(Severities);
			NewFilter.DenyList.pSeverityList = Severities;
			NewFilter.DenyList.NumIDs = ARRAYSIZE(DenyIds);
			NewFilter.DenyList.pIDList = DenyIds;

			GFX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false));
			GFX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
			GFX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
			info_queue->PushStorageFilter(&NewFilter);

			Ref<ID3D12InfoQueue1> info_queue1;
			info_queue.As(&info_queue1);
			if (info_queue1)
			{
				auto MessageCallback = [](
					D3D12_MESSAGE_CATEGORY Category,
					D3D12_MESSAGE_SEVERITY Severity,
					D3D12_MESSAGE_ID ID,
					LPCSTR pDescription,
					void* pContext)
				{
					ADRIA_LOG(WARNING, "D3D12 Validation Layer: %s", pDescription);
				};
				DWORD callbackCookie = 0;
				GFX_CHECK_HR(info_queue1->RegisterMessageCallback(MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &callbackCookie));
			}
		}
	}

	void GfxDevice::SetupOptions(GfxOptions const& options, uint32& dxgi_factory_flags)
	{
		if (options.aftermath) return;
		if (options.debug_device)
		{
			Ref<ID3D12Debug> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_controller.GetAddressOf()))))
			{
				debug_controller->EnableDebugLayer();
				dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 Debug Layer Enabled!");
#else
				ADRIA_LOG(WARNING, "D3D12 Debug Layer Enabled! (Release)");
#endif
			}
			else ADRIA_LOG(WARNING, "Debug Layer setup failed!");
		}
		if (options.dred)
		{
			Ref<ID3D12DeviceRemovedExtendedDataSettings1> dred_settings;
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(dred_settings.GetAddressOf()));
			if (SUCCEEDED(hr) && dred_settings != NULL)
			{
				dred_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
				dred_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
				dred_settings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 DRED Enabled!");
#else
				ADRIA_LOG(WARNING, "D3D12 DRED Enabled! (Release)");
#endif
			}
			else ADRIA_LOG(WARNING, "DRED setup failed!");
		}
		if (options.gpu_validation)
		{
			Ref<ID3D12Debug1> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_controller.GetAddressOf()))))
			{
				debug_controller->SetEnableGPUBasedValidation(true);
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 GPU Based Validation Enabled!");
#else
				ADRIA_LOG(WARNING, "D3D12 GPU Based Validation Enabled! (Release)");
#endif
			}
		}
		if (options.pix)
		{
			HMODULE pix_library = PIXLoadLatestWinPixGpuCapturerLibrary();
			if (pix_library)
			{
				pix_dll_loaded = true;
				ADRIA_LOG(INFO, "PIX dll loaded!");
			}
			else
			{
				pix_dll_loaded = false;
				ADRIA_LOG(WARNING, "Pix dll could not be loaded!");
			}
		}
	}

	void GfxDevice::CreateCommonRootSignature()
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

		CD3DX12_ROOT_PARAMETER1 root_parameters[4] = {}; //14 DWORDS = 8 * 1 DWORD for root constants + 3 * 2 DWORDS for CBVs
		root_parameters[0].InitAsConstantBufferView(0);
		root_parameters[1].InitAsConstants(8, 1);
		root_parameters[2].InitAsConstantBufferView(2);
		root_parameters[3].InitAsConstantBufferView(3);

		D3D12_ROOT_SIGNATURE_FLAGS flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

		CD3DX12_STATIC_SAMPLER_DESC static_samplers[10] = {};
		static_samplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		static_samplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		static_samplers[2].Init(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		static_samplers[2].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;

		static_samplers[3].Init(3, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		static_samplers[4].Init(4, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		static_samplers[5].Init(5, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		static_samplers[5].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;

		static_samplers[6].Init(6, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 16u, D3D12_COMPARISON_FUNC_LESS_EQUAL);
		static_samplers[7].Init(7, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0.0f, 16u, D3D12_COMPARISON_FUNC_LESS_EQUAL);

		static_samplers[8].Init(8, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		static_samplers[9].Init(9, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
		desc.Init_1_1(ARRAYSIZE(root_parameters), root_parameters, ARRAYSIZE(static_samplers), static_samplers, flags);

		Ref<ID3DBlob> signature;
		Ref<ID3DBlob> error;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, signature.GetAddressOf(), error.GetAddressOf());
		GFX_CHECK_HR(hr);
		hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(global_root_signature.GetAddressOf()));
		GFX_CHECK_HR(hr);
	}

	GfxDescriptor GfxDevice::CreateBufferView(GfxBuffer const* buffer, GfxSubresourceType view_type, GfxBufferDescriptorDesc const& view_desc, GfxBuffer const* uav_counter)
	{
		if (uav_counter) ADRIA_ASSERT(view_type == GfxSubresourceType::UAV);
		GfxBufferDesc desc = buffer->GetDesc();
		GfxFormat format = desc.format;
		GfxDescriptor heap_descriptor = AllocateDescriptorCPU(GfxDescriptorHeapType::CBV_SRV_UAV);
		switch (view_type)
		{
		case GfxSubresourceType::SRV:
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			bool is_accel_struct = false;
			if (format == GfxFormat::UNKNOWN)
			{
				if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferRaw))
				{
					// This is a Raw Buffer
					srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					srv_desc.Buffer.FirstElement = (uint32)view_desc.offset / sizeof(uint32);
					srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
					srv_desc.Buffer.NumElements = (uint32)std::min<uint64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32);
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferStructured))
				{
					// This is a Structured Buffer
					srv_desc.Format = DXGI_FORMAT_UNKNOWN;
					srv_desc.Buffer.FirstElement = (uint32)view_desc.offset / desc.stride;
					srv_desc.Buffer.NumElements = (uint32)std::min<uint64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
					srv_desc.Buffer.StructureByteStride = desc.stride;
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::AccelStruct))
				{
					is_accel_struct = true;
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
					srv_desc.RaytracingAccelerationStructure.Location = buffer->GetGpuAddress();
				}
			}
			else
			{
				// This is a Typed Buffer
				uint32_t stride = GetGfxFormatStride(format);
				srv_desc.Format = ConvertGfxFormat(format);
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srv_desc.Buffer.FirstElement = view_desc.offset / stride;
				srv_desc.Buffer.NumElements = (uint32)std::min<uint64>(view_desc.size, desc.size - view_desc.offset) / stride;
			}
			device->CreateShaderResourceView(!is_accel_struct ? buffer->GetNative() : nullptr, &srv_desc, heap_descriptor);
		}
		break;
		case GfxSubresourceType::UAV:
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;

			if (format == GfxFormat::UNKNOWN)
			{
				if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferRaw))
				{
					uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
					uav_desc.Buffer.FirstElement = (uint32)view_desc.offset / sizeof(uint32);
					uav_desc.Buffer.NumElements = (uint32)std::min<uint64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32);
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferStructured))
				{
					uav_desc.Format = DXGI_FORMAT_UNKNOWN;
					uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / desc.stride;
					uav_desc.Buffer.NumElements = (UINT)std::min<uint64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
					uav_desc.Buffer.StructureByteStride = desc.stride;
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::IndirectArgs))
				{
					uav_desc.Format = DXGI_FORMAT_R32_UINT;
					uav_desc.Buffer.FirstElement = (uint32)view_desc.offset / sizeof(uint32);
					uav_desc.Buffer.NumElements = (uint32)std::min<uint64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32);

				}
			}
			else
			{
				uint32 stride = GetGfxFormatStride(format);
				uav_desc.Format = ConvertGfxFormat(format);
				uav_desc.Buffer.FirstElement = (uint32)view_desc.offset / stride;
				uav_desc.Buffer.NumElements = (uint32)std::min<uint64>(view_desc.size, desc.size - view_desc.offset) / stride;
			}

			device->CreateUnorderedAccessView(buffer->GetNative(), uav_counter ? uav_counter->GetNative() : nullptr, &uav_desc, heap_descriptor);
		}
		break;
		case GfxSubresourceType::RTV:
		case GfxSubresourceType::DSV:
		default:
			ADRIA_ASSERT_MSG(false, "Buffer View can only be UAV or SRV!");
		}
		return heap_descriptor;
	}
	GfxDescriptor GfxDevice::CreateTextureView(GfxTexture const* texture, GfxSubresourceType view_type, GfxTextureDescriptorDesc const& view_desc)
	{
		GfxTextureDesc desc = texture->GetDesc();
		GfxFormat format = desc.format;
		switch (view_type)
		{
		case GfxSubresourceType::SRV:
		{
			GfxDescriptor descriptor = AllocateDescriptorCPU(GfxDescriptorHeapType::CBV_SRV_UAV);
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Shader4ComponentMapping = view_desc.channel_mapping;
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
			case GfxFormat::D32_FLOAT:
				srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				srv_desc.Format = ConvertGfxFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
					srv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					srv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					srv_desc.Texture1DArray.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture1DArray.MipLevels = view_desc.mip_count;
				}
				else
				{
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
					srv_desc.Texture1D.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture1D.MipLevels = view_desc.mip_count;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (HasAnyFlag(desc.misc_flags, GfxTextureMiscFlag::TextureCube))
					{
						if (desc.array_size > 6)
						{
							srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
							srv_desc.TextureCubeArray.First2DArrayFace = view_desc.first_slice;
							srv_desc.TextureCubeArray.NumCubes = std::min<uint32>(desc.array_size, view_desc.slice_count) / 6;
							srv_desc.TextureCubeArray.MostDetailedMip = view_desc.first_mip;
							srv_desc.TextureCubeArray.MipLevels = view_desc.mip_count;
						}
						else
						{
							srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
							srv_desc.TextureCube.MostDetailedMip = view_desc.first_mip;
							srv_desc.TextureCube.MipLevels = view_desc.mip_count;
						}
					}
					else
					{
						//if (texture->desc.sample_count > 1)
						//{
						//	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
						//	srv_desc.Texture2DMSArray.FirstArraySlice = firstSlice;
						//	srv_desc.Texture2DMSArray.ArraySize = sliceCount;
						//}
						//else
						srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
						srv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						srv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						srv_desc.Texture2DArray.MostDetailedMip = view_desc.first_mip;
						srv_desc.Texture2DArray.MipLevels = view_desc.mip_count;
					}
				}
				else
				{
					//if (texture->desc.sample_count > 1)
					//{
					//	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
					//}
					//else
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srv_desc.Texture2D.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture2D.MipLevels = view_desc.mip_count;
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				srv_desc.Texture3D.MostDetailedMip = view_desc.first_mip;
				srv_desc.Texture3D.MipLevels = view_desc.mip_count;
			}

			device->CreateShaderResourceView(texture->GetNative(), &srv_desc, descriptor);
			return descriptor;
		}
		break;
		case GfxSubresourceType::UAV:
		{
			GfxDescriptor descriptor = AllocateDescriptorCPU(GfxDescriptorHeapType::CBV_SRV_UAV);
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				uav_desc.Format = ConvertGfxFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
					uav_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					uav_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					uav_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
					uav_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					uav_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
					uav_desc.Texture2DArray.ArraySize = view_desc.slice_count;
					uav_desc.Texture2DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					uav_desc.Texture2D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
				uav_desc.Texture3D.MipSlice = view_desc.first_mip;
				uav_desc.Texture3D.FirstWSlice = 0;
				uav_desc.Texture3D.WSize = -1;
			}

			device->CreateUnorderedAccessView(texture->GetNative(), nullptr, &uav_desc, descriptor);
			return descriptor;
		}
		break;
		case GfxSubresourceType::RTV:
		{
			GfxDescriptor descriptor = AllocateDescriptorCPU(GfxDescriptorHeapType::RTV);
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				rtv_desc.Format = ConvertGfxFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
					rtv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					rtv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					rtv_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
					rtv_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (desc.sample_count > 1)
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
						rtv_desc.Texture2DMSArray.FirstArraySlice = view_desc.first_slice;
						rtv_desc.Texture2DMSArray.ArraySize = view_desc.slice_count;
					}
					else
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
						rtv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						rtv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						rtv_desc.Texture2DArray.MipSlice = view_desc.first_mip;
					}
				}
				else
				{
					if (desc.sample_count > 1)
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
						rtv_desc.Texture2D.MipSlice = view_desc.first_mip;
					}
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
				rtv_desc.Texture3D.MipSlice = view_desc.first_mip;
				rtv_desc.Texture3D.FirstWSlice = 0;
				rtv_desc.Texture3D.WSize = -1;
			}
			device->CreateRenderTargetView(texture->GetNative(), &rtv_desc, descriptor);
			return descriptor;
		}
		break;
		case GfxSubresourceType::DSV:
		{
			GfxDescriptor descriptor = AllocateDescriptorCPU(GfxDescriptorHeapType::DSV);
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
				break;
			default:
				dsv_desc.Format = ConvertGfxFormat(format);
				break;
			}
			if (view_desc.flags & GfxTextureDescriptorFlag_DepthReadOnly) dsv_desc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
			else dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
					dsv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					dsv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					dsv_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
					dsv_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (desc.sample_count > 1)
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
						dsv_desc.Texture2DMSArray.FirstArraySlice = view_desc.first_slice;
						dsv_desc.Texture2DMSArray.ArraySize = view_desc.slice_count;
					}
					else
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
						dsv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						dsv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						dsv_desc.Texture2DArray.MipSlice = view_desc.first_mip;
					}
				}
				else
				{
					if (desc.sample_count > 1)
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
						dsv_desc.Texture2D.MipSlice = view_desc.first_mip;
					}
				}
			}

			device->CreateDepthStencilView(texture->GetNative(), &dsv_desc, descriptor);
			return descriptor;
		}
		break;
		}
		ADRIA_UNREACHABLE();
	}

}


