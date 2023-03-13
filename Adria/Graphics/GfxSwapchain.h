#pragma once
#include "GfxFence.h"
#include "GfxFormat.h"

namespace adria
{
	class GfxDevice;

	struct GfxSwapchainDesc
	{
		uint32 width = 0;
		uint32 height = 0;
		GfxFormat backbuffer_format = GfxFormat::R8G8B8A8_UNORM_SRGB;
		bool fullscreen_windowed = false;
		bool vsync = false;
	};

	class GfxSwapchain
	{
	public:
		static constexpr uint32 BACKBUFFER_COUNT = 3;
	public:
		GfxSwapchain(GfxDevice* gfx, GfxSwapchainDesc const& desc);
		~GfxSwapchain();

		void Present();
		void OnResize(uint32 w, uint32 h);
		void SetVSync(bool enabled);

		IDXGISwapChain4* GetNative() const { return swapchain.Get(); }
		ID3D12Resource* GetBackbuffer() const { return back_buffers[backbuffer_index].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetBackbufferRTV() const { return back_buffer_rtvs[backbuffer_index]; }
		uint32 GetBackbufferIndex() const { return backbuffer_index; }

	private:
		GfxDevice* gfx = nullptr;
		ArcPtr<IDXGISwapChain4>				swapchain = nullptr;
		ArcPtr<ID3D12Resource>				back_buffers[BACKBUFFER_COUNT] = { nullptr };
		D3D12_CPU_DESCRIPTOR_HANDLE			back_buffer_rtvs[BACKBUFFER_COUNT] = {};
		bool vsync = false;

		uint32		 width;
		uint32		 height;
		uint32		 backbuffer_index;
		uint32		 last_backbuffer_index;

		GfxFence	 frame_fence;
		uint64		 frame_fence_value = 0;
		uint64       frame_fence_values[BACKBUFFER_COUNT];

	private:
		void CreateBackbuffers();
	};
}