#pragma once
#include "../Core/Definitions.h"
#include "../Utilities/HashUtil.h"

#ifdef _DEBUG
#define RG_DEBUG 1
#else
#define RG_DEBUG 0
#endif

#define RG_RES_NAME(name) RGResourceName(#name)

namespace adria
{
	struct RenderGraphResourceName
	{
		static constexpr uint64 INVALID_HASH = uint64(-1);
		
#if RG_DEBUG
		template<size_t N>
		constexpr explicit RenderGraphResourceName(char const (&_name)[N]) : hashed_name(crc64(_name)), name(_name)
		{}
#else
		template<size_t N>
		constexpr explicit RenderGraphResourceName(char const (&_name)[N]) : hashed_name(crc64(_name))
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