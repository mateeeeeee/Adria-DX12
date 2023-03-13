#pragma once
#include "GfxFence.h"

namespace adria
{
	class GfxDevice;

	struct GfxSwapchainDesc
	{
		void* window_handle = nullptr;
		uint32 width = 0;
		uint32 height = 0;
		GfxFormat backbuffer_format = GfxFormat::R8G8B8A8_UNORM_SRGB;
	};

	class GfxSwapchain
	{
	public:
		static constexpr uint32 BACKBUFFER_COUNT = 3;
	public:
		GfxSwapchain(GfxDevice* gfx, GfxSwapchainDesc const& desc) {}
		~GfxSwapchain() {}

		void Present() {}
		void OnResize(uint32 w, uint32 h) {}
		void SetVSync(bool enabled) { vsync = enabled; }

		operator IDXGISwapChain4*() const { return swapchain.Get(); }

	private:
		ArcPtr<IDXGISwapChain4>				swapchain;
		ArcPtr<ID3D12Resource>				back_buffers[BACKBUFFER_COUNT] = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE			back_buffer_rtvs[BACKBUFFER_COUNT];
		uint32 current_index = 0;
		bool vsync = false;

		GfxFence	 frame_fence;
		uint64		 frame_fence_value;
		uint64       frame_fence_values[BACKBUFFER_COUNT];
	};
}