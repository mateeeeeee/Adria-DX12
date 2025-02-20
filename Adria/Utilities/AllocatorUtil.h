#pragma once
#include <concepts>

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

	template<typename Allocator, typename T>
	concept IsAllocator = requires(Allocator a, void* ptr, Uint64 n, Uint64 align)
	{
		{ a.Allocate(n, align) } -> std::same_as<void*>;  // Generic allocate returning void*
		{ a.template Allocate<T>(n) } -> std::same_as<T*>; // Typed allocate returning T*
		{ a.Deallocate(ptr, n) };
	};

}