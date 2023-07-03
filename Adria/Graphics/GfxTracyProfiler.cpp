#include "GfxTracyProfiler.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"

namespace adria
{
	namespace
	{
		TracyD3D12Ctx _tracy_ctx = nullptr;
	}

	void GfxTracyProfiler::Initialize(GfxDevice* gfx)
	{
		_tracy_ctx = TracyD3D12Context(gfx->GetDevice(), gfx->GetCommandQueue(GfxCommandListType::Graphics));
	}

	void GfxTracyProfiler::Destroy()
	{
		TracyD3D12Destroy(_tracy_ctx);
	}

	void GfxTracyProfiler::NewFrame()
	{
		TracyD3D12Collect(_tracy_ctx);
		TracyD3D12NewFrame(_tracy_ctx);
	}

	TracyD3D12Ctx GfxTracyProfiler::GetCtx()
	{
		return _tracy_ctx;
	}

}


