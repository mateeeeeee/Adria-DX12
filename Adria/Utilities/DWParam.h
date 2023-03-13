#pragma once
#include <minwindef.h>

namespace adria
{
	struct DWParam
	{
		DWParam(FLOAT f) : Float(f) {}
		DWParam(UINT u) : Uint(u) {}

		void operator=(FLOAT f) { Float = f; }
		void operator=(UINT u) { Uint = u; }

		union
		{
			FLOAT Float;
			UINT Uint;
		};
	};
}