#pragma once
#include "../Events/IEvent.h"
#include "../Core/Definitions.h"

namespace adria
{

	struct ResizeEvent : IEvent
	{
		u32 width;
		u32 height;

		ResizeEvent(u32 width, u32 height) : width(width), height(height)
		{}

		virtual EventTypeID GetEventTypeID() const override
		{
			return EventTypeIdGenerator::type<ResizeEvent>;
		}
	};

	struct ScrollEvent : IEvent
	{
		i32 scroll;

		ScrollEvent(i32 scroll) : scroll(scroll)
		{}

		virtual EventTypeID GetEventTypeID() const override
		{
			return EventTypeIdGenerator::type<ScrollEvent>;
		}
	};

	

}