#pragma once
#include <cstdint>
#ifdef __cpp_concepts
#include <concepts>
#else 
#include <type_traits>
#endif

namespace adria
{

	class IEvent;

#ifdef __cpp_concepts
	template<typename T>
	concept IsEvent = std::derived_from<T, IEvent>;
#else 
	template<typename T>
	inline constexpr bool IsEvent = std::is_base_of_v<IEvent, T> && std::is_convertible_v<const volatile T*, const volatile IEvent*>;
#endif

	using EventTypeID = uint32_t;
	using EventListenerID = uint32_t;

	class IEvent
	{
		friend class EventQueue;
	public:

		virtual EventTypeID GetEventTypeID() const = 0;
		virtual ~IEvent() = default;

	protected:

		class EventTypeIdGenerator
		{
			inline static EventTypeID counter{};
		public:
#ifdef __cpp_concepts
			template<typename Type> requires IsEvent<Type>
				inline static const EventTypeID type = counter++;
#else 
			template<typename Type, typename = std::enable_if_t<IsEvent<Type>>>
			inline static const EventTypeID type = counter++;
#endif
		};

	};

}


