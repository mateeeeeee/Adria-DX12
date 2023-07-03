#pragma once
#include "GfxDefines.h"
#include "tracy/TracyD3D12.hpp"

namespace adria
{
	class GfxDevice;
	namespace GfxTracyProfiler
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();
		void NewFrame();
		TracyD3D12Ctx GetCtx();
	};
	#define g_TracyGfxCtx GfxTracyProfiler::GetCtx()

	
#if GFX_PROFILING_USE_TRACY
	#define TracyGfxProfileScope(cmd_list, name)				TracyD3D12ZoneTransient(g_TracyGfxCtx, ___tracy_gpu_zone, cmd_list, name, true)
	#define TracyGfxProfileCondScope(cmd_list, name, active)	TracyD3D12ZoneTransient(g_TracyGfxCtx, ___tracy_gpu_zone, cmd_list, name, active)
	#else
	#define TracyGfxProfileScope(cmd_list, name) 
	#define TracyGfxProfileCondScope(cmd_list, name, active) 
#endif
}