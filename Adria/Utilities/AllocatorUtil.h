#pragma once
#include "../Core/Definitions.h"
#include "../Core/Macros.h"

namespace adria
{

	inline constexpr U64 AlignToPowerOfTwo(U64 address, U64 align)
	{
		if ((0 == align) || (align & (align - 1))) return address;

		return ((address + (align - 1)) & ~(align - 1));
	}

	inline constexpr U64 Align(U64 address, U64 align)
	{
		if (align == 0 || align == 1) return address;
		U64 r = address % align; 
		return r ? address + (align - r) : address;
	}



	using OffsetType = size_t;

	inline constexpr OffsetType const INVALID_OFFSET = static_cast<OffsetType>(-1);

}