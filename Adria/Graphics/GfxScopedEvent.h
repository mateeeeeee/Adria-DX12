#pragma once
#include "GfxMacros.h"


namespace adria
{
	class GfxCommandList;
	struct GfxScopedEvent
	{
		GfxScopedEvent(GfxCommandList* cmd_list, Char const* event_name);
		~GfxScopedEvent();
		GfxCommandList* cmd_list;
	};

#if GFX_PROFILING
#define AdriaGfxScopedEvent(cmd_list, name) GfxScopedEvent ADRIA_CONCAT(__GfxEvent, __COUNTER__)(cmd_list, name)
#else
#define AdriaGfxScopedEvent(cmd_list, name) 
#endif
	inline constexpr Uint32 GfxEventColor(Uint8 r, Uint8 g, Uint8 b) { return 0xff000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b; }
}