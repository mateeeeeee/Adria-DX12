#pragma once
#include "../Events/IEvent.h"
#include "../Core/Definitions.h"

namespace adria
{

	struct ResizeEvent : IEvent
	{
		U32 width;
		U32 height;

		ResizeEvent(U32 width, U32 height) : width(width), height(height)
		{}

		virtual EventTypeID GetEventTypeID() const override
		{
			return EventTypeIdGenerator::type<ResizeEvent>;
		}
	};

	struct ScrollEvent : IEvent
	{
		I32 scroll;

		ScrollEvent(I32 scroll) : scroll(scroll)
		{}

		virtual EventTypeID GetEventTypeID() const override
		{
			return EventTypeIdGenerator::type<ScrollEvent>;
		}
	};

	

}