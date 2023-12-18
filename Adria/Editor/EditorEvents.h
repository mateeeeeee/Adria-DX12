#pragma once
#include "Utilities/Delegate.h"

namespace adria
{
	DECLARE_EVENT(TakeScreenshotEvent, Editor, char const*)
	struct EditorEvents
	{
		TakeScreenshotEvent take_screenshot_event;
	};
}