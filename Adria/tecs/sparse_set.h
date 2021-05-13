#pragma once
#include "entity.h"
#include <vector>
#include <cassert>

namespace adria::tecs
{

	class sparse_set
	{
	public:
		using size_type = size_t;
		using iterator = std::vector<entity>::iterator;
		using const_iterator = std::vector<entity>::const_iterator;
		using reverse_iterator = std::vector<entity>::reverse_iterator;
		using const_reverse_iterator = std::vector<entity>::const_reverse_iterator;

	public:
		sparse_set() = default;
		sparse_set(sparse_set const&) = default;
		sparse_set(sparse_set&&) = default;
		sparse_set& operator=(sparse_set const&) = default;
		sparse_set& operator=(sparse_set&&) = default;
		virtual ~sparse_set() = default;

		size_type size() const
		{
			return packed_array.size();
		}

		bool empty() const
		{
			return packed_array.empty();
		}

		void emplace(entity e)
		{
			auto pos = packed_array.size();
			auto index = get_index(e);

			packed_array.push_back(e);

			if (sparse_array.size() <= index)
				sparse_array.resize(index + 1);

			sparse_array[index] = pos;
		}

		bool contains(entity e) const
		{
			auto index = get_index(e);

			return index < sparse_array.size() && sparse_array[index] < packed_array.size()
				&& packed_array[sparse_array[index]] == e;
		}

		virtual void remove(entity e)
		{
			if (!contains(e)) return;
			auto index = as_integer(e);
			auto last = as_integer(packed_array.back());
			std::swap(packed_array.back(), packed_array[sparse_array[index]]);
			std::swap(sparse_array[last], sparse_array[index]);
			packed_array.pop_back();
		}

		virtual void clear()
		{
			packed_array.clear();
		}

		entity at(size_type pos) const
		{
			return pos < packed_array.size() ? packed_array[pos] : null_entity;
		}

		entity operator[](size_type pos) const
		{
			assert(pos < packed_array.size());
			return packed_array[pos];
		}

		size_type index(entity e) const
		{
			assert(contains(e));
			return sparse_array[get_index(e)];
		}

		iterator begin()
		{
			return packed_array.begin();
		}

		const_iterator begin() const
		{
			return packed_array.begin();
		}

		iterator end()
		{
			return packed_array.end();
		}

		const_iterator end() const
		{
			return packed_array.end();
		}

		reverse_iterator rbegin()
		{
			return reverse_iterator(end());
		}

		const_reverse_iterator rbegin() const
		{
			return const_reverse_iterator(end());
		}

		reverse_iterator rend()
		{
			return reverse_iterator(begin());
		}

		const_reverse_iterator rend() const
		{
			return const_reverse_iterator(begin());
		}

		const_iterator cbegin() const
		{
			return begin();
		}

		const_iterator cend() const
		{
			return end();
		}

		const_reverse_iterator crbegin() const
		{
			return rbegin();
		}

		const_reverse_iterator crend() const
		{
			return rend();
		}

	private:
		std::vector<size_type>	sparse_array;
		std::vector<entity>		packed_array;
	};

}