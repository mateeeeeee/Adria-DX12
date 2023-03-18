#include "GfxSwapchain.h"
#include "GfxDevice.h"
#include "GfxCommandQueue.h"
#include "GfxCommandList.h"
#include "../Core/Window.h"

namespace adria
{

	GfxSwapchain::GfxSwapchain(GfxDevice* gfx, GfxSwapchainDesc const& desc)
		: gfx(gfx), width(desc.width), height(desc.height)
	{
		DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
		swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapchain_desc.BufferCount = GFX_BACKBUFFER_COUNT;
		swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchain_desc.Format = ConvertGfxFormat(desc.backbuffer_format);
		swapchain_desc.Width = width;
		swapchain_desc.Height = height;
		swapchain_desc.Scaling = DXGI_SCALING_NONE;
		swapchain_desc.Stereo = FALSE;
		swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchain_desc.SampleDesc.Count = 1;
		swapchain_desc.SampleDesc.Quality = 0;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc{};
		fullscreen_desc.RefreshRate.Denominator = 60;
		fullscreen_desc.RefreshRate.Numerator = 1;
		fullscreen_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		fullscreen_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fullscreen_desc.Windowed = desc.fullscreen_windowed;

		GfxCommandQueue& graphics_queue = gfx->GetCommandQueue(GfxCommandListType::Graphics);
		ArcPtr<IDXGISwapChain1> swapchain1 = nullptr;

		BREAK_IF_FAILED(gfx->GetFactory()->CreateSwapChainForHwnd(
			graphics_queue,
			static_cast<HWND>(Window::Handle()),
			&swapchain_desc,
			&fullscreen_desc,
			nullptr,
			swapchain1.GetAddressOf()));

		swapchain.Reset();
		swapchain1.As(&swapchain);
		frame_fence.Create(gfx, "Frame Fence");

		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		last_backbuffer_index = backbuffer_index;

		CreateBackbuffers();
	}

	GfxSwapchain::~GfxSwapchain()
	{
		frame_fence.Wait(frame_fence_values[backbuffer_index]);
	}

	void GfxSwapchain::SetBackbuffer(ID3D12GraphicsCommandList* cmd_list)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetBackbufferRTV();
		cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	}

	void GfxSwapchain::ClearBackbuffer(ID3D12GraphicsCommandList* cmd_list)
	{
		float const clear_color[] = { 0,0,0,0 };
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetBackbufferRTV();
		cmd_list->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
	}

	void GfxSwapchain::Present(bool vsync)
	{
		swapchain->Present(vsync, 0);

		GfxCommandQueue& graphics_queue = gfx->GetCommandQueue(GfxCommandListType::Graphics);
		frame_fence_values[backbuffer_index] = frame_fence_value;
		graphics_queue.Signal(frame_fence, frame_fence_value);
		++frame_fence_value;

		last_backbuffer_index = backbuffer_index;
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		frame_fence.Wait(frame_fence_values[backbuffer_index]);
	}

	void GfxSwapchain::OnResize(uint32 w, uint32 h)
	{
		width = w;
		height = h;

		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			back_buffers[i].Reset();
			frame_fence_values[i] = frame_fence_values[backbuffer_index];
		}

		DXGI_SWAP_CHAIN_DESC desc{};
		swapchain->GetDesc(&desc);
		HRESULT hr = swapchain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);
		BREAK_IF_FAILED(hr);
		
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			UINT fr = (backbuffer_index + i) % GFX_BACKBUFFER_COUNT;
			HRESULT hr = swapchain->GetBuffer(i, IID_PPV_ARGS(back_buffers[i].GetAddressOf()));
			BREAK_IF_FAILED(hr);
			gfx->GetDevice()->CreateRenderTargetView(back_buffers[fr].Get(), nullptr, back_buffer_rtvs[fr]);
		}
	}

	void GfxSwapchain::CreateBackbuffers()
	{
		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			HRESULT hr = swapchain->GetBuffer(i, IID_PPV_ARGS(back_buffers[i].GetAddressOf()));
			BREAK_IF_FAILED(hr);
			back_buffer_rtvs[i] = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			gfx->GetDevice()->CreateRenderTargetView(back_buffers[i].Get(), nullptr, back_buffer_rtvs[i]);
		}
	}

}
