/*#include "GraphicsCoreDX12_deprecated.h"
#include "../Core/Window.h"
#include "../Logging/Logger.h"

namespace adria
{
    GraphicsCoreDX12_deprecated::GraphicsCoreDX12_deprecated(void* window_handle)
    {
        HWND hwnd = static_cast<HWND>(window_handle);
        RECT rect{};
        GetClientRect(hwnd, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;

        HRESULT hr = E_FAIL;

        UINT dxgi_factory_flags = 0;
#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
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
        if (debugController != NULL)
        {
            ID3D12InfoQueue* pInfoQueue = NULL;
            device->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
            pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            //pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
            pInfoQueue->Release();
        }
#endif
        // Create command queue
        D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {};
        direct_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        direct_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        direct_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        direct_queue_desc.NodeMask = 0;
        hr = device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&direct_queue));
        BREAK_IF_FAILED(hr);

        hr = device->CreateFence(fence_values[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame_fence));
        fence_values[0]++;
        BREAK_IF_FAILED(hr);
        frame_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (frame_fence_event == nullptr)
            BREAK_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));


        Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
        dxgi_factory->EnumAdapters1(1, &pAdapter);

        D3D12MA::ALLOCATOR_DESC allocator_desc = {};
        allocator_desc.pDevice = device.Get();
        allocator_desc.pAdapter = pAdapter.Get();
        D3D12MA::Allocator* _allocator = nullptr;
        hr = D3D12MA::CreateAllocator(&allocator_desc, &_allocator);
        BREAK_IF_FAILED(hr);
        allocator.reset(_allocator);

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
        hr = dxgi_factory->CreateSwapChainForHwnd(direct_queue.Get(), hwnd, &sd, nullptr, nullptr, &_swapChain);
        hr = _swapChain->QueryInterface(IID_PPV_ARGS(&swap_chain));
        BREAK_IF_FAILED(hr);

        // Create common descriptor heaps
        render_target_heap = std::make_unique<DescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, BACKBUFFER_COUNT);
        // Create frame-resident resources:
        for (UINT fr = 0; fr < BACKBUFFER_COUNT; ++fr)
        {
            hr = swap_chain->GetBuffer(fr, IID_PPV_ARGS(&frames[fr].back_buffer));
            BREAK_IF_FAILED(hr);
            frames[fr].back_buffer_rtv = render_target_heap->GetCpuHandle(fr);
            device->CreateRenderTargetView(frames[fr].back_buffer.Get(), nullptr, frames[fr].back_buffer_rtv);

            hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frames[fr].cmd_allocator.GetAddressOf()));
            BREAK_IF_FAILED(hr);
            hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[fr].cmd_allocator.Get(), nullptr, IID_PPV_ARGS(frames[fr].cmd_list.GetAddressOf()));
            BREAK_IF_FAILED(hr);
            hr = frames[fr].cmd_list->Close();
            BREAK_IF_FAILED(hr);

            for (UINT i = 0; i < CMD_LIST_COUNT; ++i)
            {
                hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frames[fr].cmd_allocators[i].GetAddressOf()));
                BREAK_IF_FAILED(hr);
                hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[fr].cmd_allocators[i].Get(), nullptr, IID_PPV_ARGS(frames[fr].cmd_lists[i].GetAddressOf()));
                BREAK_IF_FAILED(hr);
                hr = frames[fr].cmd_lists[i]->Close();
                BREAK_IF_FAILED(hr);
            }

        }

        // Create copy queue:
        D3D12_DESCRIPTOR_HEAP_DESC shader_visible_desc = {};
        shader_visible_desc.NumDescriptors = 10000;
        shader_visible_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        shader_visible_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        for (UINT i = 0; i < BACKBUFFER_COUNT; ++i)
        {
            descriptor_allocators.emplace_back(new LinearDescriptorAllocator(device.Get(), shader_visible_desc));
            upload_buffers.emplace_back(new LinearUploadBuffer(device.Get(), 10000000));
        }

        release_queue_fence_value = 0;
        hr = device->CreateFence(release_queue_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&release_queue_fence));
        release_queue_fence_value++;
        BREAK_IF_FAILED(hr);
        release_queue_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (release_queue_event == nullptr)
            BREAK_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));

    }

    GraphicsCoreDX12_deprecated::~GraphicsCoreDX12_deprecated()
	{
		WaitForGPU();
        ProcessReleaseQueue();
		CloseHandle(frame_fence_event);
	}

    void GraphicsCoreDX12_deprecated::WaitForGPU()
    {
        // Schedule a Signal command in the queue.
        BREAK_IF_FAILED(direct_queue->Signal(frame_fence.Get(), fence_values[backbuffer_index]));

        // Wait until the fence has been processed.
        BREAK_IF_FAILED(frame_fence->SetEventOnCompletion(fence_values[backbuffer_index], frame_fence_event));
        WaitForSingleObjectEx(frame_fence_event, INFINITE, FALSE);

        // Increment the fence value for the current frame.
        fence_values[backbuffer_index]++;
    }

    void GraphicsCoreDX12_deprecated::ResizeBackbuffer(UINT w, UINT h)
    {

        if ((width != w || height != h) && width > 0 && height > 0)
        {

            width = w;
            height = h;

            WaitForGPU();

            for (uint32_t fr = 0; fr < BACKBUFFER_COUNT; ++fr)
            {
                frames[fr].back_buffer.Reset();
                fence_values[fr] = fence_values[backbuffer_index];
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

	void GraphicsCoreDX12_deprecated::GetTimestampFrequency(UINT64& frequency) const
	{
        direct_queue->GetTimestampFrequency(&frequency);
	}

	UINT GraphicsCoreDX12_deprecated::BackbufferIndex() const
    {
        return backbuffer_index;
    }

    void GraphicsCoreDX12_deprecated::SetBackbuffer(ID3D12GraphicsCommandList* cmd_list)
    {
        auto& frame_resources = GetFrameResources();
        if (cmd_list) cmd_list->OMSetRenderTargets(1, &frame_resources.back_buffer_rtv, FALSE, nullptr);
        else frame_resources.cmd_list->OMSetRenderTargets(1, &frame_resources.back_buffer_rtv, FALSE, nullptr);
    }

    void GraphicsCoreDX12_deprecated::ClearBackbuffer()
    {
        descriptor_allocators[backbuffer_index]->Clear();
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
        frame_resources.cmd_list->ResourceBarrier(1, &barrier);

        FLOAT const clear_color[] = { 0,0,0,1 };
        frame_resources.cmd_list->ClearRenderTargetView(frame_resources.back_buffer_rtv, clear_color, 0, nullptr);

        frame_resources.cmd_list->SetGraphicsRootSignature(nullptr);

        ID3D12DescriptorHeap* ppHeaps[] = { descriptor_allocators[backbuffer_index]->Heap() };
        frame_resources.cmd_list->SetDescriptorHeaps(1, ppHeaps);
    }

    void GraphicsCoreDX12_deprecated::SwapBuffers(bool vsync)
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

        ExecuteCommandLists();

        ProcessReleaseQueue();

        swap_chain->Present(vsync, 0);

        MoveToNextFrame();
    }

    ID3D12Device5* GraphicsCoreDX12_deprecated::GetDevice() const
    {
        return device.Get();
    }

    ID3D12GraphicsCommandList4* GraphicsCoreDX12_deprecated::GetDefaultCommandList() const
    {
        return GetFrameResources().cmd_list.Get();
    }

    ID3D12GraphicsCommandList4* GraphicsCoreDX12_deprecated::GetNewGraphicsCommandList() const
    {
        auto& frame_resources = GetFrameResources();

        auto i = frame_resources.cmd_list_index.load();

        ++frame_resources.cmd_list_index;

        ADRIA_ASSERT(i < CMD_LIST_COUNT && "Not enough command lists");

        // Start the command list in a default state:
        HRESULT hr = frame_resources.cmd_allocators[i]->Reset();
        BREAK_IF_FAILED(hr);
        hr = frame_resources.cmd_lists[i]->Reset(frame_resources.cmd_allocators[i].Get(), nullptr);
        BREAK_IF_FAILED(hr);

        ID3D12DescriptorHeap* ppHeaps[] = { descriptor_allocators[backbuffer_index]->Heap() };
        frame_resources.cmd_lists[i]->SetDescriptorHeaps(1, ppHeaps);

        return frame_resources.cmd_lists[i].Get();
    }

    ID3D12GraphicsCommandList4* GraphicsCoreDX12_deprecated::GetLastGraphicsCommandList() const
    {
        auto& frame_resources = GetFrameResources();
        auto i = frame_resources.cmd_list_index.load();
        return i > 0 ? frame_resources.cmd_lists[i - 1].Get() : frame_resources.cmd_list.Get();
    }

    /////////////////////////////////////////////////////////////

    void GraphicsCoreDX12_deprecated::ResetDefaultCommandList()
    {
        auto& frame_resources = GetFrameResources();
        HRESULT hr = frame_resources.cmd_allocator->Reset();
        BREAK_IF_FAILED(hr);
        hr = frame_resources.cmd_list->Reset(frame_resources.cmd_allocator.Get(), nullptr);
        BREAK_IF_FAILED(hr);
    }

    void GraphicsCoreDX12_deprecated::ExecuteDefaultCommandList()
    {

        auto& frame_resources = GetFrameResources();

        frame_resources.cmd_list->Close();

        ID3D12CommandList* cmd_list = frame_resources.cmd_list.Get();

        direct_queue->ExecuteCommandLists(1, &cmd_list);
    }

    D3D12MA::Allocator* GraphicsCoreDX12_deprecated::GetAllocator() const
    {
        return allocator.get();
    }

    void GraphicsCoreDX12_deprecated::AddToReleaseQueue(D3D12MA::Allocation* alloc)
    {
        release_queue.emplace(new ReleasableResource(alloc), release_queue_fence_value);
    }

    void GraphicsCoreDX12_deprecated::AddToReleaseQueue(ID3D12Resource* resource)
    {
        release_queue.emplace(new ReleasableResource(resource), release_queue_fence_value);
    }

    LinearDescriptorAllocator* GraphicsCoreDX12_deprecated::GetDescriptorAllocator() const
    {
        return descriptor_allocators[backbuffer_index].get();
    }

    LinearUploadBuffer* GraphicsCoreDX12_deprecated::GetUploadBuffer() const
    {
        return upload_buffers[backbuffer_index].get();
    }

    GraphicsCoreDX12_deprecated::frame_resources_t& GraphicsCoreDX12_deprecated::GetFrameResources()
    {
        return frames[backbuffer_index];
    }

    GraphicsCoreDX12_deprecated::frame_resources_t const& GraphicsCoreDX12_deprecated::GetFrameResources() const
    {
        return frames[backbuffer_index];
    }

    void GraphicsCoreDX12_deprecated::MoveToNextFrame()
    {

        UINT64 const current_fence_value = fence_values[backbuffer_index];
        BREAK_IF_FAILED(direct_queue->Signal(frame_fence.Get(), current_fence_value));

        backbuffer_index = swap_chain->GetCurrentBackBufferIndex();
        // Update the frame index.

        // If the next frame is not ready to be rendered yet, wait until it is ready.
        if (frame_fence->GetCompletedValue() < fence_values[backbuffer_index])
        {
            BREAK_IF_FAILED(frame_fence->SetEventOnCompletion(fence_values[backbuffer_index], frame_fence_event));
            WaitForSingleObjectEx(frame_fence_event, INFINITE, FALSE);
        }

        // Set the fence value for the next frame.
        fence_values[backbuffer_index] = current_fence_value + 1;
    }

    void GraphicsCoreDX12_deprecated::ProcessReleaseQueue()
    {
        while (!release_queue.empty())
        {
            if (release_queue.front().fence_value > release_queue_fence->GetCompletedValue())
                break;

            auto allocation = std::move(release_queue.front());
            allocation.Release();
            release_queue.pop();
        }

        BREAK_IF_FAILED(direct_queue->Signal(release_queue_fence.Get(), release_queue_fence_value));

        ++release_queue_fence_value;

    }

    void GraphicsCoreDX12_deprecated::ExecuteCommandLists()
    {
        auto& frame_resources = GetFrameResources();

        frame_resources.cmd_list->Close();

        std::vector<ID3D12CommandList*> cmd_lists = { frame_resources.cmd_list.Get() };

        for (UINT i = 0; i < frame_resources.cmd_list_index; ++i)
        {
            frame_resources.cmd_lists[i]->Close();
            cmd_lists.push_back(frame_resources.cmd_lists[i].Get());
        }

        direct_queue->ExecuteCommandLists(static_cast<UINT>(cmd_lists.size()), cmd_lists.data());

        frame_resources.cmd_list_index.store(0);
    }

}
*/