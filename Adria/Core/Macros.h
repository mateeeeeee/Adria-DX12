#pragma once
#include <cassert>

#define RETURN_IF_FAILED(hr)	if(FAILED(hr)){return hr;}
#define THROW_IF_FAILED(hr)		if(FAILED(hr)){throw adria::AdriaException(__LINE__, __FILE__);}
#define BREAK_IF_FAILED(hr)		if(FAILED(hr)) __debugbreak()
#define THROW_EXCEPTION(msg)	throw adria::AdriaException(__LINE__, __FILE__, msg)

#define ADRIA_ASSERT(expr) assert(expr)
#define ADRIA_ASSERT_MSG(expr, msg) assert(expr && msg)
#define ADRIA_OPTIMIZE_ON  #pragma optimize("", on)
#define ADRIA_OPTIMIZE_OFF #pragma optimize("", off)
#define ADRIA_WARNINGS_OFF #pragma(warning(push, 0))
#define ADRIA_WARNINGS_ON  #pragma(warning(pop))
#define ADRIA_DEBUG_BREAK() __debugbreak()