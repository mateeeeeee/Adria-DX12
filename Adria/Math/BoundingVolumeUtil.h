#pragma once
#include <DirectXCollision.h>
#include <vector>
#include <concepts>
#include <algorithm>

namespace adria
{
   
    template<typename V>
    concept HasPosition = requires (V v)
    {
        {v.position.x} -> std::convertible_to<float>;
        {v.position.y} -> std::convertible_to<float>;
        {v.position.z} -> std::convertible_to<float>;
    };

    template<typename Iterator> requires HasPosition<std::iter_value_t<Iterator>>
    BoundingBox AABBFromRange(Iterator begin, Iterator end)
    {
        using V = typename std::iterator_traits<Iterator>::value_type;

        auto x_extremes = std::minmax_element(begin, end,
            [](V const& lhs, V const& rhs) {
                return lhs.position.x < rhs.position.x;
            });

        auto y_extremes = std::minmax_element(begin, end,
            [](V const& lhs, V const& rhs) {
                return lhs.position.y < rhs.position.y;
            });

        auto z_extremes = std::minmax_element(begin, end,
            [](V const& lhs, V const& rhs) {
                return lhs.position.z < rhs.position.z;
            });

        Vector3 lower_left(x_extremes.first->position.x, y_extremes.first->position.y, z_extremes.first->position.z);
        Vector3 upper_right(x_extremes.second->position.x, y_extremes.second->position.y, z_extremes.second->position.z);
        Vector3 center((lower_left.x + upper_right.x) * 0.5f, (lower_left.y + upper_right.y) * 0.5f, (lower_left.z + upper_right.z) * 0.5f);
        Vector3 extents(upper_right.x - center.x, upper_right.y - center.y, upper_right.z - center.z);

        return BoundingBox(center, extents);
    }

    template<typename V>
    BoundingBox AABBFromVertices(std::vector<V> const& vertices)
    {
        return AABBFromRange(vertices.begin(), vertices.end());
    }

	static BoundingBox AABBFromPositions(std::vector<Vector3> const& positions)
	{
        auto begin = positions.begin();
        auto end = positions.end();
		auto x_extremes = std::minmax_element(begin, end,
			[](auto const& lhs, auto const& rhs) {
				return lhs.x < rhs.x;
			});

		auto y_extremes = std::minmax_element(begin, end,
			[](auto const& lhs, auto const& rhs) {
				return lhs.y < rhs.y;
			});

		auto z_extremes = std::minmax_element(begin, end,
			[](auto const& lhs, auto const& rhs) {
				return lhs.z < rhs.z;
			});

		Vector3 lower_left(x_extremes.first->x, y_extremes.first->y, z_extremes.first->z);
		Vector3 upper_right(x_extremes.second->x, y_extremes.second->y, z_extremes.second->z);
        Vector3 center((lower_left.x + upper_right.x) * 0.5f, (lower_left.y + upper_right.y) * 0.5f, (lower_left.z + upper_right.z) * 0.5f);
		Vector3 extents(upper_right.x - center.x, upper_right.y - center.y, upper_right.z - center.z);

		return BoundingBox(center, extents);
	}
}