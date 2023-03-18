#pragma once
#include "GfxFence.h"
#include "GfxFormat.h"
#include "GfxMacros.h"

namespace adria
{
	class GfxDevice;

	struct GfxSwapchainDesc
	{
		uint32 width = 0;
		uint32 height = 0;
		GfxFormat backbuffer_format = GfxFormat::R8G8B8A8_UNORM_SRGB;
		bool fullscreen_windowed = false;
	};

	class GfxSwapchain
	{
	public:
		GfxSwapchain(GfxDevice* gfx, GfxSwapchainDesc const& desc);
		~GfxSwapchain();

		void SetBackbuffer(ID3D12GraphicsCommandList* cmd_list);
		void ClearBackbuffer(ID3D12GraphicsCommandList* cmd_list);
		void Present(bool vsync);
		void OnResize(uint32 w, uint32 h);

		IDXGISwapChain4* GetNative() const { return swapchain.Get(); }
		uint32 GetBackbufferIndex() const { return backbuffer_index; }
		ID3D12Resource* GetBackbuffer() const { return back_buffers[backbuffer_index].Get(); }

	private:
		GfxDevice* gfx = nullptr;
		ArcPtr<IDXGISwapChain4>				swapchain = nullptr;
		ArcPtr<ID3D12Resource>				back_buffers[GFX_BACKBUFFER_COUNT] = { nullptr };
		D3D12_CPU_DESCRIPTOR_HANDLE			back_buffer_rtvs[GFX_BACKBUFFER_COUNT] = {};

		uint32		 width;
		uint32		 height;
		uint32		 backbuffer_index;
		uint32		 last_backbuffer_index;

		GfxFence	 frame_fence;
		uint64		 frame_fence_value = 0;
		uint64       frame_fence_values[GFX_BACKBUFFER_COUNT];

	private:
		void CreateBackbuffers();
		D3D12_CPU_DESCRIPTOR_HANDLE GetBackbufferRTV() const { return back_buffer_rtvs[backbuffer_index]; }
	};
}