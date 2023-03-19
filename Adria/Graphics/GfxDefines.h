#pragma once

#define GFX_MULTITHREADED 0 
#define GFX_PROFILING 1
#define GFX_BACKBUFFER_COUNT 3

#define GFX_USE_DEPRECATED 0
#if GFX_USE_DEPRECATED
#define GFX_DEPRECATED [[deprecated]]
#else 
#define GFX_DEPRECATED 
#endif