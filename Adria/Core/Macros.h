#pragma once
#include <cassert>



#define RETURN_IF_FAILED(hr)	if(FAILED(hr)){return hr;}
#define THROW_IF_FAILED(hr)		if(FAILED(hr)){throw adria::AppException(__LINE__, __FILE__);}
#define BREAK_IF_FAILED(hr)		if(FAILED(hr)) __debugbreak()
#define THROW_EXCEPTION(msg) throw adria::AppException(__LINE__, __FILE__, msg)

#define ADRIA_ASSERT(expr) assert(expr)
#define DEBUG_PRINT(...) {char buf[1024]; sprintf_s(buf, 1024, __VA_ARGS__);  OutputDebugStringA(buf);}

