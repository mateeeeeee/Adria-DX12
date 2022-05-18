#pragma once
#include <type_traits>
#include <concepts>

namespace adria
{

#define DEFINE_ENUM_BIT_OPERATORS(ENUMTYPE) \
static_assert(std::is_enum_v<ENUMTYPE>); \
using TYPE = std::underlying_type_t<ENUMTYPE>; \
inline ENUMTYPE  operator|(ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((TYPE)a) | ((TYPE)b)); } \
inline ENUMTYPE& operator|=(ENUMTYPE& a, ENUMTYPE b) { return (ENUMTYPE &)(((TYPE&)a) |= ((TYPE)b)); } \
inline ENUMTYPE  operator&(ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((TYPE)a) & ((TYPE)b)); } \
inline ENUMTYPE& operator&=(ENUMTYPE& a, ENUMTYPE b) { return (ENUMTYPE &)(((TYPE&)a) &= ((TYPE)b)); } \
inline ENUMTYPE  operator~(ENUMTYPE a) { return ENUMTYPE(~((TYPE)a)); } \
inline ENUMTYPE  operator^(ENUMTYPE a, ENUMTYPE b) { return ENUMTYPE(((TYPE)a) ^ ((TYPE)b)); } \
inline ENUMTYPE& operator^=(ENUMTYPE& a, ENUMTYPE b) { return (ENUMTYPE &)(((TYPE&)a) ^= ((TYPE)b)); }

	template<typename Enum> requires std::is_enum_v<Enum> 
	inline constexpr bool HasAllFlags(Enum value, Enum flags)
	{
		using T = std::underlying_type_t<Enum>;
		return (((T)value) & (T)flags) == ((T)flags);
	}

	template<typename Enum> requires std::is_enum_v<Enum>
	inline constexpr bool HasAnyFlags(Enum value, Enum flags)
	{
		using T = std::underlying_type_t<Enum>;
		return (((T)value) & (T)flags) != 0;
	}

}