#pragma once
#include "entity_view.h"
#include <memory>
#include <deque>

namespace adria::tecs
{

	class registry
	{
		using component_id_t = size_t;

		class component_id_generator
		{
			inline static component_id_t counter{};
		public:
			template<typename C>
			inline static const component_id_t type = counter++;
		};

		template<typename C>
		component_pool<C>* get_component_pool() const
		{
			static_assert(std::same_as<C, std::decay_t<C>>, "Non-decayed Component types are not allowed!");

			auto component_id = component_id_generator::template type<C>;
			if (component_id >= pools.size()) pools.resize(component_id + 1);

			if (auto&& pool = pools[component_id]; !pool)
				pool.reset(new component_pool<C>());

			return static_cast<component_pool<C>*>(pools[component_id].get());
		}

		template<typename C>
		component_pool<C> const* get_component_pool_if_exists() const
		{
			static_assert(std::same_as<C, std::decay_t<C>>, "Non-decayed Component types are not allowed!");
			auto component_id = component_id_generator::template type<C>;
			return (!(component_id < pools.size()) || !pools[component_id]) ? nullptr : static_cast<component_pool<C> const*>(pools[component_id].get());
		}

		entity generate_entity()
		{
			return entities.emplace_back(make_entity((index_type)entities.size()));
		}

		entity recycle_entity()
		{
			assert(next != null_entity);
			auto i = get_index(next);
			auto v = get_version(entities[i]);
			next = make_entity(get_index(entities[i]));
			return entities[i] = make_entity(i, v);
		}

		void release_entity(entity e)
		{
			auto i = get_index(e);
			auto v = get_version(e);

			entities[i] = make_entity(get_index(next), v + 1);
			next = make_entity(i);
		}

		void remove_all(entity e)
		{
			assert(valid(e));
			for (auto& pool : pools)
				if (pool && pool->contains(e)) pool->remove(e);
		}
	public:
		using size_type = size_t;

	public:
		registry() = default;
		registry(registry const&) = default;
		registry(registry&&) = default;
		registry& operator=(registry const&) = default;
		registry& operator=(registry&&) = default;
		~registry() = default;

		[[maybe_unused]]
		entity create()
		{
			return generate_entity();
		}

		void destroy(entity e)
		{
			remove_all(e);
			release_entity(e);
			e = null_entity;
		}

		template<typename... Cs>
		void destroy()
		{
			auto entities = view<Cs...>();
			std::vector<entity> to_be_destroyed;
			for (auto e : entities) to_be_destroyed.push_back(e);
			for (auto e : to_be_destroyed) destroy(e);
		}

		bool valid(entity e) const
		{
			auto pos = get_index(e);
			return (pos < entities.size() && entities[pos] == e);
		}

		size_type alive() const
		{
			auto size = entities.size();

			for (auto e = next; e != null_entity; --size)
			{
				auto index = get_index(e);
				e = make_entity(get_index(entities[index]));
			}

			return size;
		}

		size_type size() const
		{
			return entities.size();
		}

		template<typename C>
		size_type size() const
		{
			auto const* pool = get_component_pool_if_exists<C>();
			return pool ? pool->size() : size_type{ 0 };
		}

		template<typename... Cs>
		bool empty() const
		{
			if constexpr (sizeof...(Cs) == 0) return !alive();
			else return [](auto const*... pool) { return ((!pool || pool->empty()) && ...); }(get_component_pool_if_exists<Cs>()...);
		}

		template<typename... Cs>
		void clear()
		{
			if constexpr (sizeof...(Cs) == 0)
			{
				for (auto& pool : pools)
					if (pool) pool->clear();
				for (auto e : entities) release_entity(e);
				entities.clear();
				pools.clear();
			}
			else ([this](auto* pool) {pool->clear(); }(get_component_pool<Cs>()), ...);
		}

		template<typename... Cs>
		decltype(auto) get(entity e) const
		{
			if constexpr (sizeof...(Cs) == 1)
			{
				const auto* pool = get_component_pool_if_exists<std::remove_const_t<Cs>...>();
				assert(pool);
				return pool->get(e);
			}
			else return std::forward_as_tuple(get<Cs>(e)...);
		}

		template<typename... Cs>
		decltype(auto) get(entity e)
		{
			if constexpr (sizeof...(Cs) == 1)
				return (const_cast<Cs&>(get_component_pool<std::remove_const_t<Cs>>()->get(e)), ...);
			else return std::forward_as_tuple(get<Cs>(e)...);
		}

		template<typename... Cs>
		decltype(auto) get_if(entity e) const
		{
			if constexpr (sizeof...(Cs) == 1)
			{
				const auto* pool = get_component_pool_if_exists<std::remove_const_t<Cs>...>();

				return pool ? pool->get_if(e) : nullptr;
			}
			else return std::forward_as_tuple(get<Cs>(e)...);
		}

		template<typename... Cs>
		decltype(auto) get_if(entity e)
		{
			if constexpr (sizeof...(Cs) == 1)
				return (const_cast<Cs*>(get_component_pool<std::remove_const_t<Cs>>()->get_if(e)), ...);
			else return std::forward_as_tuple(get_if<Cs>(e)...);
		}

		template<typename C, typename... Args>
		void emplace(entity e, Args&&... args)
		{
			assert(valid(e));
			get_component_pool<std::remove_const_t<C>>()->emplace(e, std::forward<Args>(args)...);
		}

		template<typename C>
		void add(entity e, C const& c)
		{
			assert(valid(e));
			get_component_pool<std::remove_const_t<C>>()->add(e, c);
		}

		template<typename C, typename... Args>
		void replace(entity e, Args&&... args)
		{
			assert(valid(e));
			get_component_pool<std::remove_const_t<C>>()->replace(e, std::forward<Args>(args)...);
		}

		template<typename C>
		void replace(entity e, C const& c)
		{
			assert(valid(e));
			get_component_pool<std::remove_const_t<C>>()->replace(e, c);
		}

		template<typename C, typename... F> requires (component_updater<C, F> && ...)
			[[maybe_unused]] decltype(auto) update(entity e, F&&... func)
		{
			assert(valid(e));
			return get_component_pool<std::remove_const_t<C>>()->update(e, std::forward<F>(func)...);
		}

		template<typename... Cs>
		void remove(entity e)
		{
			assert(valid(e));
			if constexpr (sizeof...(Cs) == 0) remove_all(e);
			else  (get_component_pool<std::remove_const_t<Cs>>()->remove(e), ...);
		}

		template<typename C>
		bool has(entity e) const
		{
			return all_of<C>(e);
		}

		template<typename... Cs>
		bool all_of(entity e) const
		{
			static_assert(sizeof...(Cs) > 0, "Provide atleast one component type!");
			assert(valid(e));
			return [e](auto const*... pool) { return ((pool && pool->contains(e)) && ...); }(get_component_pool_if_exists<Cs>()...);
		}

		template<typename... Cs>
		bool any_of(entity e) const
		{
			static_assert(sizeof...(Cs) > 0, "Provide atleast one component type!");
			assert(valid(e));
			return [e](auto const*... pool) { return !((!pool || !pool->contains(e)) && ...); }(get_component_pool_if_exists<Cs>()...);
		}

		template<typename... Cs>
		entity_view<Cs...> view()
		{
			static_assert(sizeof...(Cs) > 0);
			return { *get_component_pool<std::remove_const_t<Cs>>()... };
		}

	private:
		std::vector<entity>	entities;
		entity next = null_entity;
		mutable std::vector<std::unique_ptr<sparse_set>> pools;

	};

}