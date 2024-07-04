#pragma once

namespace adria
{

	inline constexpr uint64 AlignToPowerOfTwo(uint64 address, uint64 align)
	{
		if ((0 == align) || (align & (align - 1))) return address;

		return ((address + (align - 1)) & ~(align - 1));
	}

	inline constexpr uint64 Align(uint64 address, uint64 align)
	{
		if (align == 0 || align == 1) return address;
		uint64 r = address % align; 
		return r ? address + (align - r) : address;
	}

	using OffsetType = uint64;

	inline constexpr OffsetType const INVALID_OFFSET = static_cast<OffsetType>(-1);

	struct DummyMutex
	{
		void lock() {}
		void unlock() {}
	};
}