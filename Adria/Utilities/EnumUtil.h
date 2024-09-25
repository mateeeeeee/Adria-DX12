#pragma once
#include <type_traits>
#include <concepts>
#include <bit>

namespace adria
{

	template <typename E> requires std::is_enum_v<E>
	struct EnumBitmaskOperators
	{
		static constexpr bool enable = false;
	};
	template <typename E>
	constexpr bool EnableEnumBitmaskOperators = EnumBitmaskOperators<E>::enable;

	template <typename E>
	typename std::enable_if_t<EnableEnumBitmaskOperators<E>, E> operator|(E lhs, E rhs) 
	{
		using T = std::underlying_type_t<E>;
		return static_cast<E>(static_cast<T>(lhs) | static_cast<T>(rhs));
	}
	template <typename E>
	typename std::enable_if_t<EnableEnumBitmaskOperators<E>, E> operator&(E lhs, E rhs)
	{
		using T = std::underlying_type_t<E>;
		return static_cast<E>(static_cast<T>(lhs) & static_cast<T>(rhs));
	}
	template <typename E>
	typename std::enable_if_t<EnableEnumBitmaskOperators<E>, E> operator^(E lhs, E rhs)
	{
		using T = std::underlying_type_t<E>;
		return static_cast<E>(static_cast<T>(lhs) ^ static_cast<T>(rhs));
	}
	template <typename E>
	typename std::enable_if_t<EnableEnumBitmaskOperators<E>, E> operator~(E e)
	{
		using T = std::underlying_type_t<E>;
		return static_cast<E>(~static_cast<T>(e));
	}

	template <typename E>
	typename std::enable_if_t<EnableEnumBitmaskOperators<E>, E&> operator|=(E& lhs, E rhs)
	{
		using T = std::underlying_type_t<E>;
		return lhs = static_cast<E>(static_cast<T>(lhs) | static_cast<T>(rhs));
	}
	template <typename E>
	typename std::enable_if_t<EnableEnumBitmaskOperators<E>, E&> operator&=(E& lhs, E rhs)
	{
		using T = std::underlying_type_t<E>;
		return lhs = static_cast<E>(static_cast<T>(lhs) & static_cast<T>(rhs));
	}
	template <typename E>
	typename std::enable_if_t<EnableEnumBitmaskOperators<E>, E&> operator^=(E& lhs, E rhs)
	{
		using T = std::underlying_type_t<E>;
		return lhs = static_cast<E>(static_cast<T>(lhs) ^ static_cast<T>(rhs));
	}

	template<typename Enum> requires std::is_enum_v<Enum> 
	inline constexpr bool HasAllFlags(Enum value, Enum flags)
	{
		using T = std::underlying_type_t<Enum>;
		return (((T)value) & (T)flags) == ((T)flags);
	}

	template<typename Enum> requires std::is_enum_v<Enum>
	inline constexpr bool HasAnyFlag(Enum value, Enum flags)
	{
		using T = std::underlying_type_t<Enum>;
		return (((T)value) & (T)flags) != 0;
	}

	template<typename Enum> requires std::is_enum_v<Enum>
	inline constexpr bool HasFlag(Enum value, Enum flag)
	{
		using T = std::underlying_type_t<Enum>;
		ADRIA_ASSERT(std::has_single_bit((T)flag));
		return HasAnyFlag(value, flag);
	}

#define ENABLE_ENUM_BIT_OPERATORS(EnumType) \
	template<> struct EnumBitmaskOperators<EnumType> \
	{ static constexpr bool enable = true; }
}