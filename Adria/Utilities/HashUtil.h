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

	namespace crc
	{
		//https://stackoverflow.com/questions/28675727/using-crc32-algorithm-to-hash-string-at-compile-time 
		template <uint64_t c, int32_t k = 8>
		struct CrcInternal : CrcInternal<((c & 1) ? 0xd800000000000000L : 0) ^ (c >> 1), k - 1>
		{};

		template <uint64_t c> struct CrcInternal<c, 0>
		{
			static constexpr uint64_t value = c;
		};

		template<uint64_t c>
		constexpr uint64_t CrcInternalValue = CrcInternal<c>::value;

		#define CRC64_TABLE_0(x) CRC64_TABLE_1(x) CRC64_TABLE_1(x + 128)
		#define CRC64_TABLE_1(x) CRC64_TABLE_2(x) CRC64_TABLE_2(x +  64)
		#define CRC64_TABLE_2(x) CRC64_TABLE_3(x) CRC64_TABLE_3(x +  32)
		#define CRC64_TABLE_3(x) CRC64_TABLE_4(x) CRC64_TABLE_4(x +  16)
		#define CRC64_TABLE_4(x) CRC64_TABLE_5(x) CRC64_TABLE_5(x +   8)
		#define CRC64_TABLE_5(x) CRC64_TABLE_6(x) CRC64_TABLE_6(x +   4)
		#define CRC64_TABLE_6(x) CRC64_TABLE_7(x) CRC64_TABLE_7(x +   2)
		#define CRC64_TABLE_7(x) CRC64_TABLE_8(x) CRC64_TABLE_8(x +   1)
		#define CRC64_TABLE_8(x) CrcInternalValue<x>,

		static constexpr uint64_t CRC_TABLE[] = { CRC64_TABLE_0(0) };

		constexpr uint64_t crc64_impl(const char* str, size_t N)
		{
			uint64_t val = 0xFFFFFFFFFFFFFFFFL;
			for (size_t idx = 0; idx < N; ++idx)
			{
				val = (val >> 8) ^ CRC_TABLE[(val ^ str[idx]) & 0xFF];
			}

			return val;
		}
	}

	template<size_t N>
	constexpr uint64_t crc64(char const (&_str)[N])
	{
		return crc::crc64_impl(_str, N - 1);
	}

	constexpr uint64_t crc64(std::string const& _str)
	{
		return crc::crc64_impl(_str.c_str(), _str.size());
	}
}