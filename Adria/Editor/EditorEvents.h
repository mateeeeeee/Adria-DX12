#pragma once
#include "Utilities/Delegate.h"

namespace adria
{
	DECLARE_EVENT(TakeScreenshotEvent, Editor, char const*)
	DECLARE_EVENT(LightChangedEvent, Editor)
	struct EditorEvents
	{
		TakeScreenshotEvent take_screenshot_event;
		LightChangedEvent light_changed_event;
	};
}