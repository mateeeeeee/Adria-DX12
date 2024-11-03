#pragma once
#include "Utilities/HashUtil.h"

#ifdef _DEBUG
#define RG_DEBUG 1
#else
#define RG_DEBUG 0
#endif

#if RG_DEBUG
#define RG_NAME(name) RGResourceName(#name, adria::crc64(#name)) 
#define RG_NAME_IDX(name, idx) RGResourceName(#name, adria::crc64(#name) + idx)
#else 
#define RG_NAME(name) RGResourceName(adria::crc64(#name)) 
#define RG_NAME_IDX(name, idx) RGResourceName(adria::crc64(#name) + idx)
#endif


namespace adria
{
	struct RenderGraphResourceName
	{
		static constexpr Uint64 INVALID_HASH = Uint64(-1);

#if RG_DEBUG
		RenderGraphResourceName() : hashed_name(INVALID_HASH), name{"uninitialized"}{}
		template<Uint64 N>
		constexpr explicit RenderGraphResourceName(Char const (&_name)[N], Uint64 hash) : hashed_name(hash), name(_name)
		{}

		operator Char const*() const
		{
			return name;
		}
#else
		RenderGraphResourceName() : hashed_name(INVALID_HASH) {}
		constexpr explicit RenderGraphResourceName(Uint64 hash) : hashed_name(hash)
		{}

		operator Char const*() const
		{
			return "";
		}
#endif
		Bool IsValidName() const
		{
			return hashed_name != INVALID_HASH;
		}

		Uint64 hashed_name;
#if RG_DEBUG
		Char const* name;
#endif
	};
	using RGResourceName = RenderGraphResourceName;
	inline Bool operator==(RenderGraphResourceName const& name1, RenderGraphResourceName const& name2)
	{
		return name1.hashed_name == name2.hashed_name;
	}
}

namespace std
{
	template <> struct hash<adria::RenderGraphResourceName>
	{
		adria::Uint64 operator()(adria::RenderGraphResourceName const& res_name) const
		{
			return hash<decltype(res_name.hashed_name)>()(res_name.hashed_name);
		}
	};
}