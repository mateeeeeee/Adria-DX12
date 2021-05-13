#pragma once
#include <functional>


namespace adria
{
	template <typename T>
	constexpr void HashCombine(size_t& seed, T const& v)
	{
		std::hash<T> hasher{};
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
}