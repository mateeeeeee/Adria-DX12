#include <map>
#include <dxgidebug.h>
#include "GraphicsDeviceDX12.h"
#include "pix3.h"
#include "../Logging/Logger.h"
#include "../Core/Window.h"


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

			Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
			if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred)))) ADRIA_LOG(ERROR, "Failed to get DRED interface");
			else LogDredInfo(device, dred.Get());
			std::exit(1);
		}
		inline void ReportLiveObjects()
		{
			Microsoft::WRL::ComPtr<IDXGIDebug1> dxgi_debug;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgi_debug.GetAddressOf()))))
			{
				dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
			}
		}
	}

	GraphicsDevice::GraphicsDevice(GraphicsOptions const& options)
		: frame_fence_value(0), frame_index(0),
		frame_fence_values{}, graphics_fence_values{}, compute_fence_values{}
	{
		HWND hwnd = static_cast<HWND>(Window::Handle());
		width = Window::Width();
		height = Window::Height();

		HRESULT hr = E_FAIL;
		UINT dxgi_factory_flags = 0;

		if (options.debug_layer)
		{
			Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
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
			Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred_settings;
			hr = D3D12GetDebugInterface(IID_PPV_ARGS(&dred_settings));
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
			Microsoft::WRL::ComPtr<ID3D12Debug1> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
			{
				debug_controller->SetEnableGPUBasedValidation(true);
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 GPU Based Validation Enabled");
#else
				ADRIA_LOG(WARNING, "D3D12 GPU Based Validation Enabled in Release Mode");
#endif	
			}
		}

		Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory = nullptr;
		hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
		BREAK_IF_FAILED(hr);

		Microsoft::WRL::ComPtr<IDXGIAdapter> warp_adapter;
		BREAK_IF_FAILED(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

		hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
		BREAK_IF_FAILED(hr);

		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		dxgi_factory->EnumAdapters1(1, &adapter);

		D3D12MA::ALLOCATOR_DESC allocator_desc{};
		allocator_desc.pDevice = device.Get();
		allocator_desc.pAdapter = adapter.Get();
		D3D12MA::Allocator* _allocator = nullptr;
		BREAK_IF_FAILED(D3D12MA::CreateAllocator(&allocator_desc, &_allocator));
		allocator.reset(_allocator);

		// Create command queues
		{
			D3D12_COMMAND_QUEUE_DESC graphics_queue_desc = {};
			graphics_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			graphics_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			graphics_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			graphics_queue_desc.NodeMask = 0;
			hr = device->CreateCommandQueue(&graphics_queue_desc, IID_PPV_ARGS(&graphics_queue));
			BREAK_IF_FAILED(hr);
			graphics_queue->SetName(L"Graphics Queue");

			D3D12_COMMAND_QUEUE_DESC compute_queue_desc = {};
			compute_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
			compute_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			compute_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			compute_queue_desc.NodeMask = 0;
			hr = device->CreateCommandQueue(&compute_queue_desc, IID_PPV_ARGS(&compute_queue));
			BREAK_IF_FAILED(hr);
			compute_queue->SetName(L"Compute Queue");
		}

		//create swap chain
		{
			IDXGISwapChain1* _swapChain = nullptr;
			DXGI_SWAP_CHAIN_DESC1 sd = {};
			sd.Width = width;
			sd.Height = height;
			sd.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
			sd.Stereo = false;
			sd.SampleDesc.Count = 1; // Don't use multi-sampling.
			sd.SampleDesc.Quality = 0;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.BufferCount = BACKBUFFER_COUNT;
			sd.Flags = 0;
			sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
			sd.Scaling = DXGI_SCALING_NONE;
			hr = dxgi_factory->CreateSwapChainForHwnd(graphics_queue.Get(), hwnd, &sd, nullptr, nullptr, &_swapChain);
			hr = _swapChain->QueryInterface(IID_PPV_ARGS(&swap_chain));
			BREAK_IF_FAILED(hr);
			_swapChain->Release();

			backbuffer_index = swap_chain->GetCurrentBackBufferIndex();
			last_backbuffer_index = backbuffer_index;
		}

		//create upload and descriptor allocators
		{
			D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc{};
			shader_visible_desc.NumDescriptors = 10000;
			shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			descriptor_allocator = std::make_unique<RingOnlineDescriptorAllocator>(device.Get(), shader_visible_desc);
			for (size_t i = 0; i < offline_descriptor_allocators.size(); ++i)
			{
				offline_descriptor_allocators[i] = std::make_unique<OfflineDescriptorAllocator>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE(i), D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 250);
			}

			for (UINT i = 0; i < BACKBUFFER_COUNT; ++i)
			{
				dynamic_allocators.emplace_back(new LinearDynamicAllocator(device.Get(), 50'000'000));
			}
			dynamic_allocator_before_rendering.reset(new LinearDynamicAllocator(device.Get(), 750'000'000));
		}

		//release queue
		{
			release_queue_fence_value = 0;
			hr = device->CreateFence(release_queue_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&release_queue_fence));
			release_queue_fence_value++;
			BREAK_IF_FAILED(hr);
			release_queue_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (release_queue_event == nullptr) BREAK_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
		}

		//frame resources
		{
			for (UINT fr = 0; fr < BACKBUFFER_COUNT; ++fr)
			{
				hr = swap_chain->GetBuffer(fr, IID_PPV_ARGS(&frames[fr].back_buffer));
				BREAK_IF_FAILED(hr);
				frames[fr].back_buffer_rtv = offline_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->AllocateDescriptor();
				device->CreateRenderTargetView(frames[fr].back_buffer.Get(), nullptr, frames[fr].back_buffer_rtv);

				hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frames[fr].default_cmd_allocator.GetAddressOf()));
				BREAK_IF_FAILED(hr);
				hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[fr].default_cmd_allocator.Get(), nullptr, IID_PPV_ARGS(frames[fr].default_cmd_list.GetAddressOf()));
				BREAK_IF_FAILED(hr);
				hr = frames[fr].default_cmd_list->Close();
				BREAK_IF_FAILED(hr);

				for (UINT i = 0; i < CMD_LIST_COUNT; ++i)
				{
					hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frames[fr].cmd_allocators[i].GetAddressOf()));
					BREAK_IF_FAILED(hr);
					hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[fr].cmd_allocators[i].Get(), nullptr, IID_PPV_ARGS(frames[fr].cmd_lists[i].GetAddressOf()));
					BREAK_IF_FAILED(hr);
					hr = frames[fr].cmd_lists[i]->Close();
					BREAK_IF_FAILED(hr);

					hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(frames[fr].compute_cmd_allocators[i].GetAddressOf()));
					BREAK_IF_FAILED(hr);
					hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, frames[fr].compute_cmd_allocators[i].Get(), nullptr, IID_PPV_ARGS(frames[fr].compute_cmd_lists[i].GetAddressOf()));
					BREAK_IF_FAILED(hr);
					hr = frames[fr].compute_cmd_lists[i]->Close();
					BREAK_IF_FAILED(hr);
				}
			}
		}

		//sync objects
		{
			for (UINT i = 0; i < BACKBUFFER_COUNT; ++i)
			{
				hr = device->CreateFence(frame_fence_values[i], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame_fences[i]));
				BREAK_IF_FAILED(hr);
				frame_fence_events[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (frame_fence_events[i] == nullptr) BREAK_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));

				hr = device->CreateFence(graphics_fence_values[i], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&graphics_fences[i]));
				BREAK_IF_FAILED(hr);
				graphics_fence_events[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (graphics_fence_events[i] == nullptr) BREAK_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));

				hr = device->CreateFence(compute_fence_values[i], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&compute_fences[i]));
				BREAK_IF_FAILED(hr);
				compute_fence_events[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (compute_fence_events[i] == nullptr) BREAK_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			wait_fence_value = 0;
			hr = device->CreateFence(wait_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&wait_fence));
			wait_fence_value++;
			BREAK_IF_FAILED(hr);
			wait_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (wait_event == nullptr) BREAK_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
		}

		//Info queue
		{
			Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
			if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(pInfoQueue.GetAddressOf()))))
			{
				//D3D12_MESSAGE_CATEGORY Categories[] = {};
		
				D3D12_MESSAGE_SEVERITY Severities[] =
				{
					D3D12_MESSAGE_SEVERITY_INFO
				};
		
				D3D12_MESSAGE_ID DenyIds[] =
				{
					D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
				};
		
				D3D12_INFO_QUEUE_FILTER NewFilter = {};
				//NewFilter.DenyList.NumCategories = ARRAYSIZE(Categories);
				//NewFilter.DenyList.pCategoryList = Categories;
				NewFilter.DenyList.NumSeverities = ARRAYSIZE(Severities);
				NewFilter.DenyList.pSeverityList = Severities;
				NewFilter.DenyList.NumIDs = ARRAYSIZE(DenyIds);
				NewFilter.DenyList.pIDList = DenyIds;
		
				BREAK_IF_FAILED(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
				BREAK_IF_FAILED(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
				BREAK_IF_FAILED(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
				pInfoQueue->PushStorageFilter(&NewFilter);
		
				Microsoft::WRL::ComPtr<ID3D12InfoQueue1> pInfoQueue1;
				pInfoQueue.As(&pInfoQueue1);
				if (pInfoQueue1)
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
					BREAK_IF_FAILED(pInfoQueue1->RegisterMessageCallback(MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &callbackCookie));
				}
			}
		}
		std::atexit(ReportLiveObjects);
		
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dred_fence));
		device_removed_event = ::CreateEvent(nullptr, false, false, nullptr);
		hr = dred_fence->SetEventOnCompletion(UINT64_MAX, device_removed_event);
		if (FAILED(hr) || device_removed_event == nullptr)
		{
			ADRIA_LOG(WARNING, "Failed to set device removed completion event!");
			return;
		}
		RegisterWaitForSingleObject(&wait_handle, device_removed_event, DeviceRemovedHandler, device.Get(), INFINITE, 0);
	}

	GraphicsDevice::~GraphicsDevice()
	{
		WaitForGPU();
		ProcessReleaseQueue();

		ADRIA_ASSERT(UnregisterWaitEx(wait_handle, INVALID_HANDLE_VALUE));
		CloseHandle(device_removed_event);

		for (size_t i = 0; i < BACKBUFFER_COUNT; ++i)
		{
			if (graphics_fences[i]->GetCompletedValue() < graphics_fence_values[i])
			{
				BREAK_IF_FAILED(graphics_fences[i]->SetEventOnCompletion(graphics_fence_values[i], graphics_fence_events[i]));
				WaitForSingleObject(graphics_fence_events[i], INFINITE);
			}

			if (compute_fences[i]->GetCompletedValue() < compute_fence_values[i])
			{
				BREAK_IF_FAILED(compute_fences[i]->SetEventOnCompletion(compute_fence_values[i], compute_fence_events[i]));
				WaitForSingleObject(compute_fence_events[i], INFINITE);
			}

			CloseHandle(compute_fence_events[i]);
			CloseHandle(graphics_fence_events[i]);
			CloseHandle(frame_fence_events[i]);
		}
	}

	void GraphicsDevice::WaitForGPU()
	{
		BREAK_IF_FAILED(graphics_queue->Signal(wait_fence.Get(), wait_fence_value));
		BREAK_IF_FAILED(wait_fence->SetEventOnCompletion(wait_fence_value, wait_event));
		WaitForSingleObject(wait_event, INFINITE);
		wait_fence_value++;
	}

	void GraphicsDevice::WaitOnQueue(EQueueType type, UINT64 fence_value)
	{
		switch (type)
		{
		case EQueueType::Graphics:
			graphics_queue->Wait(compute_fences[backbuffer_index].Get(), fence_value);
			break;
		case EQueueType::Compute:
			compute_queue->Wait(graphics_fences[backbuffer_index].Get(), fence_value);
			break;
		default:
			ADRIA_ASSERT(false && "Unsupported Queue Type!");
		}
	}

	UINT64 GraphicsDevice::SignalFromQueue(EQueueType type)
	{
		UINT64 fence_signal_value = -1;
		switch (type)
		{
		case EQueueType::Graphics:
			fence_signal_value = graphics_fence_values[backbuffer_index];
			graphics_queue->Signal(graphics_fences[backbuffer_index].Get(), fence_signal_value);
			++graphics_fence_values[backbuffer_index];
			break;
		case EQueueType::Compute:
			fence_signal_value = compute_fence_values[backbuffer_index];
			compute_queue->Signal(compute_fences[backbuffer_index].Get(), compute_fence_values[backbuffer_index]);
			++compute_fence_values[backbuffer_index];
			break;
		default:
			ADRIA_ASSERT(false && "Unsupported Queue Type!");
		}
		return fence_signal_value;
	}

	void GraphicsDevice::ResizeBackbuffer(UINT w, UINT h)
	{
		if ((width != w || height != h) && width > 0 && height > 0)
		{
			width = w;
			height = h;
			WaitForGPU();

			for (UINT fr = 0; fr < BACKBUFFER_COUNT; ++fr)
			{
				frames[fr].back_buffer.Reset();
				frame_fence_values[fr] = frame_fence_values[backbuffer_index];
			}

			DXGI_SWAP_CHAIN_DESC desc = {};
			swap_chain->GetDesc(&desc);
			HRESULT hr = swap_chain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);
			BREAK_IF_FAILED(hr);

			backbuffer_index = swap_chain->GetCurrentBackBufferIndex();
			for (UINT i = 0; i < BACKBUFFER_COUNT; ++i)
			{
				UINT fr = (backbuffer_index + i) % BACKBUFFER_COUNT;
				hr = swap_chain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)frames[fr].back_buffer.GetAddressOf());
				BREAK_IF_FAILED(hr);
				device->CreateRenderTargetView(frames[fr].back_buffer.Get(), nullptr, frames[fr].back_buffer_rtv);
			}
		}
	}

	UINT GraphicsDevice::BackbufferIndex() const
	{
		return backbuffer_index;
	}

	UINT GraphicsDevice::FrameIndex() const { return frame_index; }

	void GraphicsDevice::SetBackbuffer(ID3D12GraphicsCommandList* cmd_list /*= nullptr*/)
	{
		auto& frame_resources = GetFrameResources();
		if (cmd_list) cmd_list->OMSetRenderTargets(1, &frame_resources.back_buffer_rtv, FALSE, nullptr);
		else frame_resources.default_cmd_list->OMSetRenderTargets(1, &frame_resources.back_buffer_rtv, FALSE, nullptr);
	}

	void GraphicsDevice::ClearBackbuffer()
	{
		if (rendering_not_started) [[unlikely]]
		{
			rendering_not_started = FALSE;
			dynamic_allocator_before_rendering = nullptr;
		}

		descriptor_allocator->ReleaseCompletedFrames(frame_index);
		dynamic_allocators[backbuffer_index]->Clear();

		ResetDefaultCommandList();

		auto& frame_resources = GetFrameResources();
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = frame_resources.back_buffer.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		frame_resources.default_cmd_list->ResourceBarrier(1, &barrier);

		FLOAT const clear_color[] = { 0,0,0,0 };
		frame_resources.default_cmd_list->ClearRenderTargetView(frame_resources.back_buffer_rtv, clear_color, 0, nullptr);
		frame_resources.default_cmd_list->SetGraphicsRootSignature(nullptr);
		ID3D12DescriptorHeap* ppHeaps[] = { descriptor_allocator->Heap() };
		frame_resources.default_cmd_list->SetDescriptorHeaps(1, ppHeaps);
	}

	void GraphicsDevice::SwapBuffers(bool vsync /*= false*/)
	{
		auto& frame_resources = GetFrameResources();

		HRESULT hr = S_OK;

		// Indicate that the back buffer will now be used to present.
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = frame_resources.back_buffer.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

		UINT cmd_list_index = frame_resources.cmd_list_index;

		GetLastGraphicsCommandList()->ResourceBarrier(1, &barrier);

		ExecuteGraphicsCommandLists();
		ExecuteComputeCommandLists();

		ProcessReleaseQueue();

		swap_chain->Present(vsync, 0);

		MoveToNextFrame();

		descriptor_allocator->FinishCurrentFrame(frame_index);
	}

	ID3D12Device5* GraphicsDevice::GetDevice() const
	{
		return device.Get();
	}

	ID3D12GraphicsCommandList4* GraphicsDevice::GetDefaultCommandList() const
	{
		return GetFrameResources().default_cmd_list.Get();
	}

	ID3D12GraphicsCommandList4* GraphicsDevice::GetNewGraphicsCommandList() const
	{
		auto& frame_resources = GetFrameResources();

		unsigned int i = frame_resources.cmd_list_index.load();

		++frame_resources.cmd_list_index;

		ADRIA_ASSERT(i < CMD_LIST_COUNT && "Not enough command lists");

		HRESULT hr = frame_resources.cmd_allocators[i]->Reset();
		BREAK_IF_FAILED(hr);
		hr = frame_resources.cmd_lists[i]->Reset(frame_resources.cmd_allocators[i].Get(), nullptr);
		BREAK_IF_FAILED(hr);

		ID3D12DescriptorHeap* ppHeaps[] = { descriptor_allocator->Heap() };
		frame_resources.cmd_lists[i]->SetDescriptorHeaps(1, ppHeaps);

		return frame_resources.cmd_lists[i].Get();
	}

	ID3D12GraphicsCommandList4* GraphicsDevice::GetLastGraphicsCommandList() const
	{
		auto& frame_resources = GetFrameResources();
		unsigned int i = frame_resources.cmd_list_index.load();
		return i > 0 ? frame_resources.cmd_lists[i - 1].Get() : frame_resources.default_cmd_list.Get();
	}

	ID3D12GraphicsCommandList4* GraphicsDevice::GetNewComputeCommandList() const
	{
		auto& frame_resources = GetFrameResources();

		unsigned int i = frame_resources.compute_cmd_list_index.load();

		++frame_resources.compute_cmd_list_index;

		ADRIA_ASSERT(i < CMD_LIST_COUNT && "Not enough command lists");

		HRESULT hr = frame_resources.compute_cmd_allocators[i]->Reset();
		BREAK_IF_FAILED(hr);
		hr = frame_resources.compute_cmd_lists[i]->Reset(frame_resources.compute_cmd_allocators[i].Get(), nullptr);
		BREAK_IF_FAILED(hr);

		ID3D12DescriptorHeap* ppHeaps[] = { descriptor_allocator->Heap() };
		frame_resources.compute_cmd_lists[i]->SetDescriptorHeaps(1, ppHeaps);

		return frame_resources.compute_cmd_lists[i].Get();
	}

	ID3D12GraphicsCommandList4* GraphicsDevice::GetLastComputeCommandList() const
	{
		auto& frame_resources = GetFrameResources();
		unsigned int i = frame_resources.compute_cmd_list_index.load();
		return i > 0 ? frame_resources.compute_cmd_lists[i - 1].Get() : frame_resources.default_cmd_list.Get();
	}

	void GraphicsDevice::ResetDefaultCommandList()
	{
		auto& frame_resources = GetFrameResources();
		HRESULT hr = frame_resources.default_cmd_allocator->Reset();
		BREAK_IF_FAILED(hr);
		hr = frame_resources.default_cmd_list->Reset(frame_resources.default_cmd_allocator.Get(), nullptr);
		BREAK_IF_FAILED(hr);
	}

	void GraphicsDevice::ExecuteDefaultCommandList()
	{
		auto& frame_resources = GetFrameResources();
		frame_resources.default_cmd_list->Close();
		ID3D12CommandList* cmd_list = frame_resources.default_cmd_list.Get();
		graphics_queue->ExecuteCommandLists(1, &cmd_list);
	}

	D3D12MA::Allocator* GraphicsDevice::GetAllocator() const
	{
		return allocator.get();
	}

	void GraphicsDevice::AddToReleaseQueue(D3D12MA::Allocation* alloc)
	{
		release_queue.emplace(new ReleasableResource(alloc), release_queue_fence_value);
	}

	void GraphicsDevice::AddToReleaseQueue(ID3D12Resource* resource)
	{
		release_queue.emplace(new ReleasableResource(resource), release_queue_fence_value);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDevice::AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		return offline_descriptor_allocators[type]->AllocateDescriptor();
	}

	void GraphicsDevice::FreeOfflineDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		offline_descriptor_allocators[type]->FreeDescriptor(handle);
	}

	RingOnlineDescriptorAllocator* GraphicsDevice::GetOnlineDescriptorAllocator() const
	{
		return descriptor_allocator.get();
	}

	void GraphicsDevice::ReserveOnlineDescriptors(size_t reserve)
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc{};
		shader_visible_desc.NumDescriptors = 10000;
		shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptor_allocator = std::make_unique<RingOnlineDescriptorAllocator>(device.Get(), shader_visible_desc, reserve);
	}

	LinearDynamicAllocator* GraphicsDevice::GetDynamicAllocator() const
	{
		if (rendering_not_started) return dynamic_allocator_before_rendering.get();
		else return dynamic_allocators[backbuffer_index].get();
	}

	void GraphicsDevice::GetTimestampFrequency(UINT64& frequency) const
	{
		graphics_queue->GetTimestampFrequency(&frequency);
	}

	GraphicsDevice::FrameResources& GraphicsDevice::GetFrameResources()
	{
		return frames[backbuffer_index];
	}

	GraphicsDevice::FrameResources const& GraphicsDevice::GetFrameResources() const
	{
		return frames[backbuffer_index];
	}

	void GraphicsDevice::ExecuteGraphicsCommandLists()
	{
		auto& frame_resources = GetFrameResources();
		frame_resources.default_cmd_list->Close();
		std::vector<ID3D12CommandList*> cmd_lists = { frame_resources.default_cmd_list.Get() };
		for (UINT i = 0; i < frame_resources.cmd_list_index; ++i)
		{
			frame_resources.cmd_lists[i]->Close();
			cmd_lists.push_back(frame_resources.cmd_lists[i].Get());
		}
		graphics_queue->ExecuteCommandLists(static_cast<UINT>(cmd_lists.size()), cmd_lists.data());
		frame_resources.cmd_list_index.store(0);
	}

	void GraphicsDevice::ExecuteComputeCommandLists()
	{
		auto& frame_resources = GetFrameResources();
		if (frame_resources.compute_cmd_list_index == 0) return;
		std::vector<ID3D12CommandList*> cmd_lists = {};
		for (UINT i = 0; i < frame_resources.compute_cmd_list_index; ++i)
		{
			frame_resources.compute_cmd_lists[i]->Close();
			cmd_lists.push_back(frame_resources.compute_cmd_lists[i].Get());
		}
		compute_queue->ExecuteCommandLists(static_cast<UINT>(cmd_lists.size()), cmd_lists.data());
		frame_resources.compute_cmd_list_index.store(0);
	}

	void GraphicsDevice::MoveToNextFrame()
	{
		// Assign the current fence value to the current frame.
		frame_fence_values[backbuffer_index] = frame_fence_value;
		// Signal and increment the fence value.
		BREAK_IF_FAILED(graphics_queue->Signal(frame_fences[backbuffer_index].Get(), frame_fence_value));
		++frame_fence_value;

		// Update the frame index.
		last_backbuffer_index = backbuffer_index;
		backbuffer_index = swap_chain->GetCurrentBackBufferIndex();
		if (frame_fences[backbuffer_index]->GetCompletedValue() < frame_fence_values[backbuffer_index])
		{
			BREAK_IF_FAILED(frame_fences[backbuffer_index]->SetEventOnCompletion(frame_fence_values[backbuffer_index], frame_fence_events[backbuffer_index]));
			WaitForSingleObject(frame_fence_events[backbuffer_index], INFINITE);
		}
		++frame_index;
	}

	void GraphicsDevice::ProcessReleaseQueue()
	{
		while (!release_queue.empty())
		{
			if (release_queue.front().fence_value > release_queue_fence->GetCompletedValue()) break;
			release_queue.pop();
		}
		BREAK_IF_FAILED(graphics_queue->Signal(release_queue_fence.Get(), release_queue_fence_value));
		++release_queue_fence_value;
	}
}


