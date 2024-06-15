#include "GfxSwapchain.h"
#include "GfxDevice.h"
#include "GfxCommandQueue.h"
#include "GfxCommandList.h"
#include "GfxTexture.h"
#include "Core/Window.h"

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
		Ref<IDXGISwapChain1> swapchain1 = nullptr;

		GFX_CHECK_HR(gfx->GetFactory()->CreateSwapChainForHwnd(
			graphics_queue,
			static_cast<HWND>(gfx->GetHwnd()),
			&swapchain_desc,
			&fullscreen_desc,
			nullptr,
			swapchain1.GetAddressOf()));

		swapchain.Reset();
		swapchain1.As(&swapchain);
		
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		CreateBackbuffers();
	}

	GfxSwapchain::~GfxSwapchain() {}

	void GfxSwapchain::SetAsRenderTarget(GfxCommandList* cmd_list)
	{
		GfxDescriptor rtvs[] = { GetBackbufferDescriptor() };
		cmd_list->SetRenderTargets(rtvs);
	}

	void GfxSwapchain::ClearBackbuffer(GfxCommandList* cmd_list)
	{
		constexpr float clear_color[] = { 0,0,0,0 };
		GfxDescriptor rtv = GetBackbufferDescriptor();
		cmd_list->ClearRenderTarget(rtv, clear_color);
	}

	bool GfxSwapchain::Present(bool vsync)
	{
		HRESULT hr = swapchain->Present(vsync, 0);
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		return SUCCEEDED(hr);
	}

	void GfxSwapchain::OnResize(uint32 w, uint32 h)
	{
		width = w;
		height = h;

		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			back_buffers[i].reset(nullptr);
		}

		DXGI_SWAP_CHAIN_DESC desc{};
		swapchain->GetDesc(&desc);
		HRESULT hr = swapchain->ResizeBuffers(desc.BufferCount, width, height, desc.BufferDesc.Format, desc.Flags);
		GFX_CHECK_HR(hr);
		
		backbuffer_index = swapchain->GetCurrentBackBufferIndex();
		CreateBackbuffers();
	}

	void GfxSwapchain::CreateBackbuffers()
	{
		for (uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			Ref<ID3D12Resource> backbuffer = nullptr;
			HRESULT hr = swapchain->GetBuffer(i, IID_PPV_ARGS(backbuffer.GetAddressOf()));
			GFX_CHECK_HR(hr);
			D3D12_RESOURCE_DESC desc = backbuffer->GetDesc();
			GfxTextureDesc gfx_desc{};
			gfx_desc.width = (uint32)desc.Width;
			gfx_desc.height = (uint32)desc.Height;
			gfx_desc.format = ConvertDXGIFormat(desc.Format);
			gfx_desc.initial_state = GfxResourceState::Present;
			gfx_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
			gfx_desc.bind_flags = GfxBindFlag::RenderTarget;
			back_buffers[i] = gfx->CreateBackbufferTexture(gfx_desc, backbuffer);
			back_buffers[i]->SetName("Backbuffer");
			backbuffer_rtvs[i] = gfx->CreateTextureRTV(back_buffers[i].get());
		}
	}

	GfxDescriptor GfxSwapchain::GetBackbufferDescriptor() const
	{
		return backbuffer_rtvs[backbuffer_index];
	}

}
