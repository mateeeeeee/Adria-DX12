#pragma once
#include <cstdint>
#include <utility>
#include <type_traits>
#include <concepts>

namespace adria::tecs
{

    enum class entity : uint64_t {};
    using index_type    = uint32_t;
    using version_type  = uint32_t;


    inline constexpr entity make_entity(index_type index, version_type version = 0)
    {
        using underlying_type = std::underlying_type_t<entity>;

        return entity(index + (static_cast<underlying_type>(version) << 32));
    }

    inline constexpr auto as_integer(entity e)
    {
        using underlying_type = std::underlying_type_t<entity>;

        return static_cast<underlying_type>(e);
    }

    inline constexpr version_type get_index(entity e)
    {
        auto integer = as_integer(e);

        return static_cast<index_type>(integer);
    }

    inline constexpr version_type get_version(entity e)
    {
        auto integer = as_integer(e);

        return static_cast<version_type>(integer >> 32);
    }

    inline constexpr std::pair<index_type, version_type> split_entity(entity e)
    {
        auto integer = as_integer(e);

        return std::make_pair(static_cast<index_type>(integer), static_cast<version_type>(integer << 32));
    }

    inline constexpr entity null_entity = make_entity(static_cast<index_type>(-1));

}