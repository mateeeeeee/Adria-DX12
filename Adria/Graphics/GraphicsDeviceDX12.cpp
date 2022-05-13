#include "GraphicsDeviceDX12.h"

namespace adria
{

	GraphicsDevice::GraphicsDevice(void* window_handle)
		: frame_fence_value(0), frame_index(0),
		frame_fence_values{}, graphics_fence_values{}, compute_fence_values{}
	{
		HWND hwnd = static_cast<HWND>(window_handle);
		RECT rect{};
		GetClientRect(hwnd, &rect);
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;

		HRESULT hr = E_FAIL;
		UINT dxgi_factory_flags = 0;
#if defined(_DEBUG)
		Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
		{
			debug_controller->EnableDebugLayer();
			dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif

		Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory = nullptr;
		hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
		BREAK_IF_FAILED(hr);

		Microsoft::WRL::ComPtr<IDXGIAdapter> warp_adapter;
		BREAK_IF_FAILED(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

		hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
		BREAK_IF_FAILED(hr);

#if defined(_DEBUG)
		if (debug_controller != NULL)
		{
			ID3D12InfoQueue* pInfoQueue = NULL;
			device->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			pInfoQueue->Release();
		}
#endif
		Microsoft::WRL::ComPtr<IDXGIAdapter1> p_adapter;
		dxgi_factory->EnumAdapters1(1, &p_adapter);

		D3D12MA::ALLOCATOR_DESC allocator_desc{};
		allocator_desc.pDevice = device.Get();
		allocator_desc.pAdapter = p_adapter.Get();
		D3D12MA::Allocator* _allocator = nullptr;
		hr = D3D12MA::CreateAllocator(&allocator_desc, &_allocator);
		BREAK_IF_FAILED(hr);
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

			backbuffer_index = swap_chain->GetCurrentBackBufferIndex();
			last_backbuffer_index = backbuffer_index;
		}

		//create upload and descriptor allocators
		{
			D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc{};
			shader_visible_desc.NumDescriptors = 10000;
			shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			descriptor_allocator = std::make_unique<RingDescriptorAllocator>(device.Get(), shader_visible_desc);
			for (UINT i = 0; i < BACKBUFFER_COUNT; ++i)
			{
				upload_buffers.emplace_back(new LinearUploadBuffer(device.Get(), 10000000));
			}
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

		render_target_heap = std::make_unique<DescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, BACKBUFFER_COUNT);

		//frame resources
		{
			for (UINT fr = 0; fr < BACKBUFFER_COUNT; ++fr)
			{
				hr = swap_chain->GetBuffer(fr, IID_PPV_ARGS(&frames[fr].back_buffer));
				BREAK_IF_FAILED(hr);
				frames[fr].back_buffer_rtv = render_target_heap->GetHandle(fr);
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
	}

	GraphicsDevice::~GraphicsDevice()
	{
		WaitForGPU();
		ProcessReleaseQueue();
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

			for (uint32_t fr = 0; fr < BACKBUFFER_COUNT; ++fr)
			{
				frames[fr].back_buffer.Reset();
				frame_fence_values[fr] = frame_fence_values[backbuffer_index];
			}

			DXGI_SWAP_CHAIN_DESC desc = {};
			swap_chain->GetDesc(&desc);

			HRESULT hr = swap_chain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);
			BREAK_IF_FAILED(hr);

			backbuffer_index = swap_chain->GetCurrentBackBufferIndex();
			for (uint32_t i = 0; i < BACKBUFFER_COUNT; ++i)
			{
				uint32_t fr = (backbuffer_index + i) % BACKBUFFER_COUNT;

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
		descriptor_allocator->ReleaseCompletedFrames(frame_index);
		upload_buffers[backbuffer_index]->Clear();

		ResetDefaultCommandList();

		auto& frame_resources = GetFrameResources();
		D3D12_RESOURCE_BARRIER barrier = {};
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

	RingDescriptorAllocator* GraphicsDevice::GetDescriptorAllocator() const
	{
		return descriptor_allocator.get();
	}

	void GraphicsDevice::ReserveDescriptors(size_t reserve)
	{
		D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc{};
		shader_visible_desc.NumDescriptors = 10000;
		shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptor_allocator = std::make_unique<RingDescriptorAllocator>(device.Get(), shader_visible_desc, reserve);
	}

	LinearUploadBuffer* GraphicsDevice::GetUploadBuffer() const
	{
		return upload_buffers[backbuffer_index].get();
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
			if (release_queue.front().fence_value > release_queue_fence->GetCompletedValue())
				break;

			auto allocation = std::move(release_queue.front());
			allocation.Release();
			release_queue.pop();
		}

		BREAK_IF_FAILED(graphics_queue->Signal(release_queue_fence.Get(), release_queue_fence_value));

		++release_queue_fence_value;
	}

}


