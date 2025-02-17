#pragma once
#include "GfxMacros.h"
#include "GfxCommandList.h"
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxTracyProfiler.h"
#include "Graphics/GfxNsightPerfManager.h"
#include "pix3.h"

#define GFX_SCOPE(cmd_list, name) \
    PIXScopedEvent(cmd_list->GetNative(), PIX_COLOR_DEFAULT, name); \
    AdriaGfxProfileScope(cmd_list, name); \
    NsightPerfGfxRangeScope(cmd_list, name); \
    TracyGfxProfileScope(cmd_list->GetNative(), name)
