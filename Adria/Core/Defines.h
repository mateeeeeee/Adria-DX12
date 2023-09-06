#pragma once
#include <cassert>

#define _ADRIA_STRINGIFY_IMPL(a) #a
#define _ADRIA_CONCAT_IMPL(x, y) x##y

#define ADRIA_STRINGIFY(a) _ADRIA_STRINGIFY_IMPL(a)
#define ADRIA_CONCAT(x, y) _ADRIA_CONCAT_IMPL( x, y )

#define ADRIA_ASSERT(expr)			assert(expr)
#define ADRIA_ASSERT_MSG(expr, msg) assert(expr && msg)
#define ADRIA_OPTIMIZE_ON			pragma optimize("", on)
#define ADRIA_OPTIMIZE_OFF			pragma optimize("", off)
#define ADRIA_WARNINGS_OFF			pragma(warning(push, 0))
#define ADRIA_WARNINGS_ON			pragma(warning(pop))
#define ADRIA_DEBUGBREAK()			__debugbreak()
#define ADRIA_UNREACHABLE()			__assume(false)
#define ADRIA_FORCEINLINE			__forceinline
#define ADRIA_INLINE				inline
#define ADRIA_NODISCARD				[[nodiscard]]
#define ADRIA_NORETURN				[[noreturn]]
#define ADRIA_DEPRECATED			[[deprecated]]
#define ADRIA_DEPRECATED_MSG(msg)	[[deprecated(#msg)]]
#define ADRIA_ALIGN(align)           alignas(align) 



#define ADRIA_NONCOPYABLE(ClassName)                 \
    ClassName(ClassName const&)            = delete; \
    ClassName& operator=(ClassName const&) = delete;

#define ADRIA_NONMOVABLE(ClassName)                      \
    ClassName(ClassName&&) noexcept            = delete; \
    ClassName& operator=(ClassName&&) noexcept = delete;

#define ADRIA_NONCOPYABLE_NONMOVABLE(ClassName) \
        ADRIA_NONCOPYABLE(ClassName)                \
        ADRIA_NONMOVABLE(ClassName)

#define ADRIA_DEFAULT_COPYABLE(ClassName)             \
    ClassName(ClassName const&)            = default; \
    ClassName& operator=(ClassName const&) = default;

#define ADRIA_DEFAULT_MOVABLE(ClassName)                  \
    ClassName(ClassName&&) noexcept            = default; \
    ClassName& operator=(ClassName&&) noexcept = default;

#define ADRIA_DEFAULT_COPYABLE_MOVABLE(ClassName) \
    ADRIA_DEFAULT_COPYABLE(ClassName)             \
    ADRIA_DEFAULT_MOVABLE(ClassName)
