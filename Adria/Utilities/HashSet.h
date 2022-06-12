#pragma once
#if _DEBUG
#include <unordered_set>
#else 
#include "tsl/robin_set.h"
#endif

namespace adria
{
#if _DEBUG
	template<typename K>
	using HashSet = std::unordered_set<K>;
#else 
	template<typename K>
	using HashSet = tsl::robin_set<K>;
#endif
}