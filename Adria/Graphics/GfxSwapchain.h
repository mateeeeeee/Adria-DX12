#pragma once
#include <memory>
#include "GfxFormat.h"
#include "GfxMacros.h"
#include "GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class GfxTexture;

	struct GfxSwapchainDesc
	{
		Uint32 width = 0;
		Uint32 height = 0;
		GfxFormat backbuffer_format = GfxFormat::R8G8B8A8_UNORM_SRGB;
		Bool fullscreen_windowed = false;
	};

	class GfxSwapchain
	{
	public:
		GfxSwapchain(GfxDevice* gfx, GfxSwapchainDesc const& desc);
		~GfxSwapchain();

		void SetAsRenderTarget(GfxCommandList* cmd_list);
		void ClearBackbuffer(GfxCommandList* cmd_list);
		Bool Present(Bool vsync);
		void OnResize(Uint32 w, Uint32 h);

		Uint32 GetBackbufferIndex() const { return backbuffer_index; }
		GfxTexture* GetBackbuffer() const { return back_buffers[backbuffer_index].get(); }
		
	private:
		GfxDevice* gfx = nullptr;
		Ref<IDXGISwapChain4>				swapchain = nullptr;
		std::unique_ptr<GfxTexture>			back_buffers[GFX_BACKBUFFER_COUNT] = { nullptr };
		GfxDescriptor					    backbuffer_rtvs[GFX_BACKBUFFER_COUNT];
		Uint32		 width;
		Uint32		 height;
		Uint32		 backbuffer_index;

	private:
		void CreateBackbuffers();
		GfxDescriptor GetBackbufferDescriptor() const;
	};
}