#pragma once
#include <functional>

namespace adria
{
	template <typename T>
	constexpr void HashCombine(uint64& seed, T const& v)
	{
		std::hash<T> hasher{};
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	namespace crc
	{
		//https://stackoverflow.com/questions/28675727/using-crc32-algorithm-to-hash-string-at-compile-time 
		template <uint64 c, uint32 k = 8>
		struct CrcInternal : CrcInternal<((c & 1) ? 0xd800000000000000L : 0) ^ (c >> 1), k - 1>
		{};

		template <uint64 c> struct CrcInternal<c, 0>
		{
			static constexpr uint64 value = c;
		};

		template<uint64 c>
		constexpr uint64 CrcInternalValue = CrcInternal<c>::value;

		#define CRC64_TABLE_0(x) CRC64_TABLE_1(x) CRC64_TABLE_1(x + 128)
		#define CRC64_TABLE_1(x) CRC64_TABLE_2(x) CRC64_TABLE_2(x +  64)
		#define CRC64_TABLE_2(x) CRC64_TABLE_3(x) CRC64_TABLE_3(x +  32)
		#define CRC64_TABLE_3(x) CRC64_TABLE_4(x) CRC64_TABLE_4(x +  16)
		#define CRC64_TABLE_4(x) CRC64_TABLE_5(x) CRC64_TABLE_5(x +   8)
		#define CRC64_TABLE_5(x) CRC64_TABLE_6(x) CRC64_TABLE_6(x +   4)
		#define CRC64_TABLE_6(x) CRC64_TABLE_7(x) CRC64_TABLE_7(x +   2)
		#define CRC64_TABLE_7(x) CRC64_TABLE_8(x) CRC64_TABLE_8(x +   1)
		#define CRC64_TABLE_8(x) CrcInternalValue<x>,

		static constexpr uint64 CRC_TABLE[] = { CRC64_TABLE_0(0) };

		constexpr uint64 crc64_impl(const char* str, uint64 N)
		{
			uint64 val = 0xFFFFFFFFFFFFFFFFL;
			for (uint64 idx = 0; idx < N; ++idx)
			{
				val = (val >> 8) ^ CRC_TABLE[(val ^ str[idx]) & 0xFF];
			}

			return val;
		}
	}

	//guaranteed compile time using consteval
	template<uint64 N>
	consteval uint64 crc64(char const (&_str)[N])
	{
		return crc::crc64_impl(_str, N - 1);
	}

	inline uint64 crc64(char const* _str, uint64 N)
	{
		return crc::crc64_impl(_str, N);
	}
}