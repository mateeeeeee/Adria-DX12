#include "EventQueue.h"

namespace adria
{

	EventListenerID EventQueue::GetEventListenerID()
	{
		static EventListenerID listener_id = 0;
		return listener_id++;
	}

	void EventQueue::PushEvent(IEvent* event)
	{
		events.push(event);
	}

	void EventQueue::TriggerEvent(IEvent* event)
	{
		auto& _listeners = listeners[event->GetEventTypeID()];

		for (auto const& [id, listener] : _listeners) listener(event);
	}

	void EventQueue::Unsubscribe(EventListenerID listener_id, EventTypeID event_id)
	{
		auto& _listeners = listeners[event_id];

		for (auto it = _listeners.begin(); it != _listeners.end(); ++it)
		{
			if (it->first == listener_id)
			{
				_listeners.erase(it);
				return;
			}
		}
	}

	void EventQueue::ProcessEvents()
	{
		while (!events.empty())
		{
			auto event = events.front();

			events.pop();

			auto& _listeners = listeners[event->GetEventTypeID()];

			for (auto const& [id, listener] : _listeners) listener(event);
		}
	}

	void EventQueue::Unsubscribe(EventListenerID listener_id) //without eventtypeid searches the whole map for listener
	{
		for (auto& [event_id, _listeners] : listeners)
		{
			for (auto it = _listeners.begin(); it != _listeners.end(); ++it)
			{
				if (it->first == listener_id)
				{
					_listeners.erase(it);
					return;
				}
			}
		}
	}

}