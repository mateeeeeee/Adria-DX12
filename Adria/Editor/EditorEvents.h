#pragma once
#include "Events/Delegate.h"

namespace adria
{
	DECLARE_EVENT(TakeScreenshotEvent, Editor, char const*);
	struct EditorEvents
	{
		TakeScreenshotEvent take_screenshot_event;
	};
}