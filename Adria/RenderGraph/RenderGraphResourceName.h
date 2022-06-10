#pragma once
#include "../Core/Definitions.h"
#include "../Utilities/HashUtil.h"

#ifdef _DEBUG
#define RG_DEBUG 1
#else
#define RG_DEBUG 0
#endif
#define RG_IDENTITY(a) a
#define RG_CONCAT(a,b) a##b
#define RG_STRINGIFY(c) #c
#define RG_RES_NAME(name) RGResourceName(#name) 
#define RG_RES_NAME_IDX(name, idx) RGResourceName(#name, idx)
#define RG_STR_RES_NAME(name) RGResourceName(name)
#define RG_STR_RES_NAME_IDX(name, idx) RGResourceName(name, idx)


namespace adria
{
	struct RenderGraphResourceName
	{
		static constexpr uint64 INVALID_HASH = uint64(-1);

#if RG_DEBUG
		RenderGraphResourceName() : hashed_name(INVALID_HASH), name{"uninitialized"}{}
		template<size_t N>
		constexpr explicit RenderGraphResourceName(char const (&_name)[N], uint64 idx = 0) : hashed_name(crc64(_name) + idx), name(_name)
		{}
#else
		RenderGraphResourceName() : hashed_name(INVALID_HASH) {}
		template<size_t N>
		constexpr explicit RenderGraphResourceName(char const (&_name)[N], uint64 idx = 0) : hashed_name(crc64(_name) + idx)
		{}
#endif
		uint64 hashed_name;
#if RG_DEBUG
		char const* name;
#endif
	};
	using RGResourceName = RenderGraphResourceName;

	inline bool operator==(RenderGraphResourceName const& name1, RenderGraphResourceName const& name2)
	{
		return name1.hashed_name == name2.hashed_name;
	}
}

namespace std
{
	template <> struct hash<adria::RenderGraphResourceName>
	{
		size_t operator()(adria::RenderGraphResourceName const& res_name) const
		{
			return hash<decltype(res_name.hashed_name)>()(res_name.hashed_name);
		}
	};
}