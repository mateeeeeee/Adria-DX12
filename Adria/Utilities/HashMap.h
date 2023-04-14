#pragma once
#if _DEBUG
#include <unordered_map>
#else 
#include <unordered_map>
#endif

namespace adria
{
#if _DEBUG
	template<typename K, typename V>
	using HashMap = std::unordered_map<K, V>;
#else 
	template<typename K, typename V>
	using HashMap = std::unordered_map<K, V>; //change later to better hash map with the same interface
#endif
}