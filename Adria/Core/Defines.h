#pragma once
#include <cassert>

#define _STRINGIFY_IMPL(a) #a
#define _CONCAT_IMPL(x, y) x##y
#define ADRIA_STRINGIFY(a) _STRINGIFY_IMPL(a)
#define ADRIA_CONCAT(x, y) _CONCAT_IMPL( x, y )

#define ADRIA_ASSERT(expr) assert(expr)
#define ADRIA_ASSERT_MSG(expr, msg) assert(expr && msg)
#define ADRIA_OPTIMIZE_ON  #pragma optimize("", on)
#define ADRIA_OPTIMIZE_OFF #pragma optimize("", off)
#define ADRIA_WARNINGS_OFF #pragma(warning(push, 0))
#define ADRIA_WARNINGS_ON  #pragma(warning(pop))
#define ADRIA_DEBUGBREAK() __debugbreak()
#define ADRIA_FORCEINLINE __forceinline
#define ADRIA_UNREACHABLE() __assume(false)

#define BREAK_IF_FAILED(hr)		if(FAILED(hr)) __debugbreak()
