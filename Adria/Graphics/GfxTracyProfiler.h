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
	#define TracyGfxProfileScope(cmd_list, name) TracyD3D12Zone(g_TracyGfxCtx, cmd_list, name)
	#define TracyGfxProfileScopeColored(cmd_list, name, color) TracyD3D12ZoneC(g_TracyGfxCtx, cmd_list, name, color)
	#define TracyGfxProfileCondScope(cmd_list, name, active) TracyD3D12NamedZone(g_TracyGfxCtx, ___tracy_gpu_zone, cmd_list, name, active)
	#define TracyGfxProfileCondScopeColored(cmd_list, name, active, color) TracyD3D12NamedZoneC(g_TracyGfxCtx, ___tracy_gpu_zone, cmd_list, name, color, active)
#else
	#define TracyGfxProfileScope(cmd_list, name) 
	#define TracyGfxProfileScopeColored(cmd_list, name, color) 
	#define TracyGfxProfileCondScope(cmd_list, name, active) 
	#define TracyGfxProfileCondScopeColored(cmd_list, name, active, color)
#endif
}