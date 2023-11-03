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
#if GFX_PROFILING_USE_TRACY
		_tracy_ctx = TracyD3D12Context(gfx->GetDevice(), gfx->GetCommandQueue(GfxCommandListType::Graphics));
#endif
	}

	void GfxTracyProfiler::Destroy()
	{
#if GFX_PROFILING_USE_TRACY
		TracyD3D12Destroy(_tracy_ctx);
#endif
	}

	void GfxTracyProfiler::NewFrame()
	{
#if GFX_PROFILING_USE_TRACY
		TracyD3D12Collect(_tracy_ctx);
		TracyD3D12NewFrame(_tracy_ctx);
#endif
	}

	TracyD3D12Ctx GfxTracyProfiler::GetCtx()
	{
		return _tracy_ctx;
	}

}


