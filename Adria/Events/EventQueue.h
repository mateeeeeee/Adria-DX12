#pragma once
#include "IEvent.h"
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace adria
{

	class EventQueue
	{

		using EventCallback = std::function<void(IEvent const* const)>;
		using EventListener = std::pair<EventListenerID, EventCallback>; //id and callback, id used to remove it
		using EventListenerList = std::vector<EventListener>; // a vector of listeners to a particular event
		using EventListenerMap = std::unordered_map<EventTypeID, EventListenerList>; // maps an event type to those events listening to it

		template<typename EventType>
		using WrapperCallback = std::function<void(EventType const&)>;

		static EventListenerID GetEventListenerID();

		template <typename EventType>
		struct CallbackWrapper
		{
			CallbackWrapper(WrapperCallback<EventType> callback) : callback(callback) {}
			void operator() (IEvent const* const event) { callback(static_cast<EventType const&>(*event)); }
			WrapperCallback<EventType> callback;
		};

	public:
		template<typename EventType, typename... Args>
		void ConstructAndPushEvent(Args&&... args);

		template<typename EventType, typename = std::enable_if_t<!std::is_pointer_v<EventType>>>
		void PushEvent(EventType const& event);

		void PushEvent(IEvent* event);

		template<typename EventType, typename... Args>
		void ConstructAndTriggerEvent(Args&&... args);

		template<typename EventType, typename = std::enable_if_t<!std::is_pointer_v<EventType>>>
		void TriggerEvent(EventType const& event);

		void TriggerEvent(IEvent* event);

		//regular function overload
		template<typename EventType>
		[[maybe_unused]] EventListenerID Subscribe(void(*callback)(EventType const&));

		//functor and lambda overload, EventType template argument has to be explicitly written when using this 
		template<typename EventType, typename F>
		[[maybe_unused]] EventListenerID Subscribe(F&& callback);

		//member functions
		template<typename T, typename EventType>
		[[maybe_unused]] EventListenerID Subscribe(void(T::* member_callback)(EventType const&), T& instance);

		template<typename EventType>
		void Unsubscribe(EventListenerID id);

		void Unsubscribe(EventListenerID listener_id, EventTypeID event_id);

		void Unsubscribe(EventListenerID listener_id);

		void ProcessEvents();

	private:
		std::queue<IEvent const*> events;
		EventListenerMap listeners;
	};




	template<typename EventType, typename ...Args>
	void EventQueue::ConstructAndPushEvent(Args && ...args)
	{
		static_assert(std::is_base_of_v<IEvent, EventType>, "EventType has to derive from IEvent");

		events.push(new EventType(std::forward<Args>(args)...));
	}

	template<typename EventType, typename>
	void EventQueue::PushEvent(EventType const& event)
	{
		static_assert(std::is_base_of_v<IEvent, EventType>, "EventType has to derive from IEvent");

		events.push(&event);
	}

	template<typename EventType, typename ...Args>
	void EventQueue::ConstructAndTriggerEvent(Args && ...args)
	{
		TriggerEvent(EventType(std::forward<Args>(args)...));
	}

	template<typename EventType, typename>
	void EventQueue::TriggerEvent(EventType const& event)
	{
		static_assert(std::is_base_of_v<IEvent, EventType>, "EventType has to derive from IEvent");

		auto& _listeners = listeners[event.GetEventTypeID()];

		for (auto const& [id, listener] : _listeners) listener(&event);
	}

	template<typename EventType>
	[[maybe_unused]]
	EventListenerID EventQueue::Subscribe(void(*callback)(EventType const&))
	{
		static_assert(std::is_base_of_v<IEvent, EventType>);

		auto& _listeners = listeners[IEvent::EventTypeIdGenerator::type<EventType>];

		EventListenerID listener_id = GetEventListenerID();

		_listeners.push_back({ listener_id, CallbackWrapper(WrapperCallback<EventType>(callback)) });

		return listener_id;
	}

	template<typename EventType, typename F>
	[[maybe_unused]]
	EventListenerID EventQueue::Subscribe(F&& callback)
	{
		auto& _listeners = listeners[IEvent::EventTypeIdGenerator::type<EventType>];

		EventListenerID listener_id = GetEventListenerID();

		_listeners.push_back({ listener_id, CallbackWrapper(WrapperCallback<EventType>(callback)) });

		return listener_id;
	}

	template<typename T, typename EventType>
	[[maybe_unused]]
	EventListenerID EventQueue::Subscribe(void(T::* member_callback)(EventType const&), T& instance)
	{
		std::function<void(EventType const& e)> callback = [&instance, member_callback](EventType const& e) mutable ->void {return (instance.*member_callback)(e); };

		EventListenerID listener_id = GetEventListenerID();

		auto& _listeners = listeners[IEvent::EventTypeIdGenerator::type<EventType>];

		_listeners.push_back({ listener_id, CallbackWrapper(WrapperCallback<EventType>(callback)) });

		return listener_id;
	}

	template<typename EventType>
	void EventQueue::Unsubscribe(EventListenerID id)
	{
		Unsubscribe(id, IEvent::EventTypeIdGenerator::type<EventType>);
	}


}