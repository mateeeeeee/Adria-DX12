#pragma once
#include "Utilities/Delegate.h"

namespace adria
{
	DECLARE_EVENT(TakeScreenshotEvent, Editor, Char const*)
	DECLARE_EVENT(LightChangedEvent, Editor)
	struct EditorEvents
	{
		TakeScreenshotEvent take_screenshot_event;
		LightChangedEvent light_changed_event;
	};
}