#pragma once

namespace adria
{

	inline constexpr Uint64 AlignToPowerOfTwo(Uint64 address, Uint64 align)
	{
		if ((0 == align) || (align & (align - 1))) return address;

		return ((address + (align - 1)) & ~(align - 1));
	}

	inline constexpr Uint64 Align(Uint64 address, Uint64 align)
	{
		if (align == 0 || align == 1) return address;
		Uint64 r = address % align; 
		return r ? address + (align - r) : address;
	}

	inline constexpr Uint64 INVALID_ALLOC_OFFSET = static_cast<Uint64>(-1);

	struct DummyMutex
	{
		void lock() {}
		void unlock() {}
	};
}