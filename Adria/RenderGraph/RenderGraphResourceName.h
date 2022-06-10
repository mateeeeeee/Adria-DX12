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
#define RG_RES_NAME_IDX(name, idx) RGResourceName(RG_CONCAT(RG_STRINGIFY(name), RG_STRINGIFY(idx)))
#define RG_STR_RES_NAME(name) RGResourceName(name)
#define RG_STR_RES_NAME_IDX(name, idx) RGResourceName(RG_CONCAT(RG_IDENTITY(name), RG_STRINGIFY(idx)))


namespace adria
{
	struct RenderGraphResourceName
	{
		static constexpr uint64 INVALID_HASH = uint64(-1);
		
#if RG_DEBUG
		template<size_t N>
		constexpr explicit RenderGraphResourceName(char const (&_name)[N]) : hashed_name(crc64(_name)), name(_name)
		{}

		constexpr explicit RenderGraphResourceName(std::string const& _name) : hashed_name(crc64(_name)), name(_name.c_str())
		{}
#else
		template<size_t N>
		constexpr explicit RenderGraphResourceName(char const (&_name)[N]) : hashed_name(crc64(_name))
		{}
		constexpr explicit RenderGraphResourceName(std::string const& _name) : hashed_name(crc64(_name))
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