#include <map>
#include <dxgidebug.h>
#include "pix3.h"
#include "GfxDevice.h"
#include "GfxSwapchain.h"
#include "GfxCommandList.h"
#include "RingGPUDescriptorAllocator.h"
#include "LinearGPUDescriptorAllocator.h"
#include "CPUDescriptorAllocator.h"
#include "LinearDynamicAllocator.h"
#include "d3dx12.h"
#include "../Logging/Logger.h"
#include "../Core/Window.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

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

			ArcPtr<ID3D12DeviceRemovedExtendedData1> dred;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(dred.GetAddressOf())))) ADRIA_LOG(ERROR, "Failed to get DRED interface");
			else LogDredInfo(device, dred.Get());
			std::exit(1);
		}
		inline void ReportLiveObjects()
		{
			ArcPtr<IDXGIDebug1> dxgi_debug;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgi_debug.GetAddressOf()))))
			{
				dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
			}
		}
	}

	GfxDevice::DRED::DRED(GfxDevice* gfx)
	{
		dred_fence.Create(gfx, "DRED Fence");
		dred_wait_handle = CreateEventA(nullptr, false, false, nullptr);
		static_cast<ID3D12Fence*>(dred_fence)->SetEventOnCompletion(UINT64_MAX, dred_wait_handle);
		ADRIA_ASSERT(RegisterWaitForSingleObject(&dred_wait_handle, dred_wait_handle, DeviceRemovedHandler, gfx->GetDevice(), INFINITE, 0));
	}

	GfxDevice::DRED::~DRED()
	{
		dred_fence.Signal(UINT64_MAX);
		ADRIA_ASSERT(UnregisterWaitEx(dred_wait_handle, INVALID_HANDLE_VALUE));
		CloseHandle(dred_wait_handle);
	}

	GfxDevice::GfxDevice(GfxOptions const& options)
		: frame_index(0)
	{
		HWND hwnd = static_cast<HWND>(Window::Handle());
		width = Window::Width();
		height = Window::Height();

		HRESULT hr = E_FAIL;
		uint32 dxgi_factory_flags = 0;
		SetupOptions(options, dxgi_factory_flags);
		hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));
		BREAK_IF_FAILED(hr);

		hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(device.GetAddressOf()));
		BREAK_IF_FAILED(hr);

		ArcPtr<IDXGIAdapter1> adapter;
		dxgi_factory->EnumAdapters1(1, adapter.GetAddressOf());

		D3D12MA::ALLOCATOR_DESC allocator_desc{};
		allocator_desc.pDevice = device.Get();
		allocator_desc.pAdapter = adapter.Get();
		D3D12MA::Allocator* _allocator = nullptr;
		BREAK_IF_FAILED(D3D12MA::CreateAllocator(&allocator_desc, &_allocator));
		allocator.reset(_allocator);

		graphics_queue.Create(this, GfxCommandListType::Graphics, "Graphics Queue");
		compute_queue.Create(this, GfxCommandListType::Compute, "Compute Queue");
		copy_queue.Create(this, GfxCommandListType::Copy, "Copy Queue");

		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			graphics_cmd_lists[i] = std::make_unique<GfxCommandList>(this, GfxCommandListType::Graphics, "graphics command list");
			compute_cmd_lists[i]  = std::make_unique<GfxCommandList>(this, GfxCommandListType::Compute, "compute command list");
			upload_cmd_lists[i]   = std::make_unique<GfxCommandList>(this, GfxCommandListType::Copy, "upload command list");
		}

		for (uint32 i = 0; i < offline_descriptor_allocators.size(); ++i) offline_descriptor_allocators[i] = std::make_unique<CPUDescriptorAllocator>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE(i), D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 250);
		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i) dynamic_allocators.emplace_back(new LinearDynamicAllocator(this, 50'000'000));
		dynamic_allocator_before_rendering.reset(new LinearDynamicAllocator(this, 750'000'000));

		GfxSwapchainDesc swapchain_desc{};
		swapchain_desc.width = width;
		swapchain_desc.height = height;
		swapchain_desc.fullscreen_windowed = true;
		swapchain_desc.backbuffer_format = GfxFormat::R10G10B10A2_UNORM;
		swapchain = std::make_unique<GfxSwapchain>(this, swapchain_desc);

		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frame_resources[i].cmd_allocator.GetAddressOf()));
			BREAK_IF_FAILED(hr);
			hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame_resources[i].cmd_allocator.Get(), nullptr, IID_PPV_ARGS(frame_resources[i].cmd_list.GetAddressOf()));
			BREAK_IF_FAILED(hr);
			hr = frame_resources[i].cmd_list->Close();
			BREAK_IF_FAILED(hr);
		}
		wait_fence.Create(this, "Wait Fence");
		release_fence.Create(this, "Release Fence");

		draw_indirect_signature = std::make_unique<DrawIndirectSignature>(device.Get());
		draw_indexed_indirect_signature = std::make_unique<DrawIndexedIndirectSignature>(device.Get());
		dispatch_indirect_signature = std::make_unique<DispatchIndirectSignature>(device.Get());

		SetInfoQueue();
		CreateCommonRootSignature();

		std::atexit(ReportLiveObjects);
		if (options.dred) dred = std::make_unique<DRED>(this);
	}

	GfxDevice::GfxDevice(GfxDevice&&) = default;

	GfxDevice::~GfxDevice()
	{
		WaitForGPU();
		ProcessReleaseQueue();
	}

	void GfxDevice::WaitForGPU()
	{
		graphics_queue.Signal(wait_fence, wait_fence_value);
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
			swapchain->OnResize(w, h);
		}
	}

	uint32 GfxDevice::BackbufferIndex() const
	{
		return swapchain->GetBackbufferIndex();
	}

	uint32 GfxDevice::FrameIndex() const { return frame_index; }

	void GfxDevice::SetBackbuffer(ID3D12GraphicsCommandList* cmd_list)
	{
		swapchain->SetBackbuffer(cmd_list);
	}

	void GfxDevice::BeginFrame()
	{
		if (rendering_not_started) [[unlikely]]
		{
			rendering_not_started = FALSE;
			dynamic_allocator_before_rendering.reset();
		}
		uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		descriptor_allocator->ReleaseCompletedFrames(frame_index);
		dynamic_allocators[backbuffer_index]->Clear();

		ResetCommandList();
		auto& frame_resources = GetFrameResources();
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = swapchain->GetBackbuffer();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		frame_resources.cmd_list->ResourceBarrier(1, &barrier);

		ID3D12DescriptorHeap* heaps[] = { descriptor_allocator->Heap() };
		frame_resources.cmd_list->SetDescriptorHeaps(1, heaps);
		frame_resources.cmd_list->SetGraphicsRootSignature(global_root_signature.Get());
		frame_resources.cmd_list->SetComputeRootSignature(global_root_signature.Get());

		swapchain->ClearBackbuffer(frame_resources.cmd_list);
	}

	void GfxDevice::EndFrame(bool vsync /*= false*/)
	{
		auto& frame_resources = GetFrameResources();

		HRESULT hr = S_OK;

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = swapchain->GetBackbuffer();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		GetCommandList()->ResourceBarrier(1, &barrier);

		ExecuteCommandLists();
		ProcessReleaseQueue();

		swapchain->Present(vsync);
		++frame_index;
		descriptor_allocator->FinishCurrentFrame(frame_index);
	}

	IDXGIFactory4* GfxDevice::GetFactory() const
	{
		return dxgi_factory.Get();
	}

	ID3D12Device5* GfxDevice::GetDevice() const
	{
		return device.Get();
	}

	ID3D12GraphicsCommandList4* GfxDevice::GetCommandList() const
	{
		return GetFrameResources().cmd_list.Get();
	}

	ID3D12RootSignature* GfxDevice::GetCommonRootSignature() const
	{
		return global_root_signature.Get();
	}

	ID3D12Resource* GfxDevice::GetBackbuffer() const
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

	GfxCommandList* GfxDevice::GetCommandList(GfxCommandListType type)
	{
		uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_lists[backbuffer_index].get();
		case GfxCommandListType::Compute:
			return compute_cmd_lists[backbuffer_index].get();
		case GfxCommandListType::Copy:
			return upload_cmd_lists[backbuffer_index].get();
		default:
			return graphics_cmd_lists[backbuffer_index].get();
		}
		ADRIA_UNREACHABLE();
	}


	void GfxDevice::ResetCommandList()
	{
		auto& frame_resources = GetFrameResources();
		HRESULT hr = frame_resources.cmd_allocator->Reset();
		BREAK_IF_FAILED(hr);
		hr = frame_resources.cmd_list->Reset(frame_resources.cmd_allocator.Get(), nullptr);
		BREAK_IF_FAILED(hr);
	}

	void GfxDevice::ExecuteCommandList()
	{
		auto& frame_resources = GetFrameResources();

		std::vector<ID3D12CommandList*> cmd_lists;
		frame_resources.cmd_list->Close();
		ID3D12CommandList* cmd_list = frame_resources.cmd_list.Get();
		cmd_lists.push_back(cmd_list);
		graphics_queue.ExecuteCommandLists(cmd_lists);
	}

	D3D12MA::Allocator* GfxDevice::GetAllocator() const
	{
		return allocator.get();
	}

	void GfxDevice::AddToReleaseQueue(D3D12MA::Allocation* alloc)
	{
		release_queue.emplace(new ReleasableResource(alloc), release_queue_fence_value);
	}

	void GfxDevice::AddToReleaseQueue(ID3D12Resource* resource)
	{
		release_queue.emplace(new ReleasableResource(resource), release_queue_fence_value);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxDevice::AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		return offline_descriptor_allocators[type]->AllocateDescriptor();
	}

	void GfxDevice::FreeOfflineDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		offline_descriptor_allocators[type]->FreeDescriptor(handle);
	}

	RingGPUDescriptorAllocator* GfxDevice::GetOnlineDescriptorAllocator() const
	{
		return descriptor_allocator.get();
	}

	void GfxDevice::ReserveOnlineDescriptors(size_t reserve)
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc{};
		shader_visible_desc.NumDescriptors = 32767;
		shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptor_allocator = std::make_unique<RingGPUDescriptorAllocator>(device.Get(), shader_visible_desc, reserve);
	}

	LinearDynamicAllocator* GfxDevice::GetDynamicAllocator() const
	{
		if (rendering_not_started) return dynamic_allocator_before_rendering.get();
		else return dynamic_allocators[swapchain->GetBackbufferIndex()].get();
	}

	void GfxDevice::GetTimestampFrequency(UINT64& frequency) const
	{
		frequency = graphics_queue.GetTimestampFrequency();
	}

	GfxDevice::FrameResources& GfxDevice::GetFrameResources()
	{
		return frame_resources[swapchain->GetBackbufferIndex()];
	}

	GfxDevice::FrameResources const& GfxDevice::GetFrameResources() const
	{
		return frame_resources[swapchain->GetBackbufferIndex()];
	}

	void GfxDevice::ExecuteCommandLists()
	{
		ExecuteCommandList();
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
		ArcPtr<ID3D12InfoQueue> info_queue;
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

			BREAK_IF_FAILED(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
			BREAK_IF_FAILED(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false));
			BREAK_IF_FAILED(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
			info_queue->PushStorageFilter(&NewFilter);

			ArcPtr<ID3D12InfoQueue1> info_queue1;
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
				BREAK_IF_FAILED(info_queue1->RegisterMessageCallback(MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &callbackCookie));
			}
		}
	}

	void GfxDevice::SetupOptions(GfxOptions const& options, uint32& dxgi_factory_flags)
	{
		if (options.debug_layer)
		{
			ArcPtr<ID3D12Debug> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_controller.GetAddressOf()))))
			{
				debug_controller->EnableDebugLayer();
				dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 Debug Layer Enabled");
#else
				ADRIA_LOG(WARNING, "D3D12 Debug Layer Enabled in Release Mode");
#endif
			}
			else ADRIA_LOG(WARNING, "debug layer setup failed!");
		}
		if (options.dred)
		{
			ArcPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred_settings;
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(dred_settings.GetAddressOf()));
			if (SUCCEEDED(hr) && dred_settings != NULL)
			{
				dred_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
				dred_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
				dred_settings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 DRED Enabled");
#else
				ADRIA_LOG(WARNING, "D3D12 DRED Enabled in Release Mode");
#endif
			}
			else ADRIA_LOG(WARNING, "Dred setup failed!");
		}
		if (options.gpu_validation)
		{
			ArcPtr<ID3D12Debug1> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_controller.GetAddressOf()))))
			{
				debug_controller->SetEnableGPUBasedValidation(true);
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 GPU Based Validation Enabled");
#else
				ADRIA_LOG(WARNING, "D3D12 GPU Based Validation Enabled in Release Mode");
#endif
			}
		}
		if (options.pix) PIXLoadLatestWinPixGpuCapturerLibrary();
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

		CD3DX12_STATIC_SAMPLER_DESC static_samplers[8] = {};
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

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
		desc.Init_1_1(ARRAYSIZE(root_parameters), root_parameters, ARRAYSIZE(static_samplers), static_samplers, flags);

		ArcPtr<ID3DBlob> signature;
		ArcPtr<ID3DBlob> error;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, signature.GetAddressOf(), error.GetAddressOf());
		BREAK_IF_FAILED(hr);
		hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(global_root_signature.GetAddressOf()));
		BREAK_IF_FAILED(hr);
	}
}


