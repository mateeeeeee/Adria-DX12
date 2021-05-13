#pragma once
#include "component_pool.h"
#include <tuple>
#include <array>
#include <algorithm>

//move to template util

namespace adria::tecs
{

    namespace details
    {
        template <typename T, typename... Ts>
        constexpr bool contains = (std::is_same<T, Ts>{} || ...);
        template <typename Subset, typename Set>
        constexpr bool is_subset_of = false;
        template <typename... Ts, typename... Us>
        constexpr bool is_subset_of<std::tuple<Ts...>, std::tuple<Us...>>
            = (contains<Ts, Us...> && ...);
    }

    template <typename F>
    concept valid_each_function = requires(entity e, F&& f) { { f(e) } ->std::same_as<void>; };

    template<typename... Cs>
    class entity_view
    {

        using unchecked_type = std::array<sparse_set const*, (sizeof...(Cs) - 1)>;

        sparse_set const* smallest_set() const
        {
            return (std::min)({ static_cast<sparse_set const*>(std::get<component_pool<Cs>*>(pools))... },
                [](auto const* Lhs, auto const* Rhs)
                {
                    return Lhs->size() < Rhs->size();
                });
        }

        unchecked_type get_unchecked(sparse_set const* pool) const
        {
            std::size_t pos{};
            unchecked_type other{};
            (static_cast<void>(std::get<component_pool<Cs>*>(pools) == pool ? 
                void() : 
                void(other[pos++] = std::get<component_pool<Cs>*>(pools))), ...);
            return other;
        }

        template<typename It> requires std::same_as<std::iter_value_t<It>, entity>
        class view_iterator
        {
            friend class entity_view<Cs...>;

            bool valid() const
            {
                const auto e = *it;
                return std::all_of(std::begin(unchecked), std::end(unchecked), [e](sparse_set const* curr) { return curr->contains(e); });
            }

            view_iterator(It from, It to, It curr, unchecked_type unchecked)
                :   first{ from },
                    last{ to },
                    it{ curr },
                    unchecked{ unchecked }
            {
                if (it != last && !valid())  ++(*this);
            }

        public:
            using difference_type = typename std::iterator_traits<It>::difference_type;
            using value_type = typename std::iterator_traits<It>::value_type;
            using pointer = typename std::iterator_traits<It>::pointer;
            using reference = typename std::iterator_traits<It>::reference;
            using iterator_category = std::bidirectional_iterator_tag;

        public:

            view_iterator() : view_iterator{ It{}, It{}, It{}, unchecked_type{} }
            {}

            view_iterator& operator++()
            {
                while (++it != last && !valid());
                return *this;
            }

            view_iterator operator++(int)
            {
                view_iterator orig = *this;
                return ++(*this), orig;
            }

            view_iterator& operator--()
            {
                while (--it != first && !valid());
                return *this;
            }

            view_iterator operator--(int)
            {
                view_iterator orig = *this;
                return operator--(), orig;
            }

            bool operator==(view_iterator const& other) const
            {
                return other.it == it;
            }

            bool operator!=(view_iterator const& other) const
            {
                return !(*this == other);
            }

            pointer operator->() const {
                return &*it;
            }

            reference operator*() const {
                return *operator->();
            }

        private:
            It first;
            It last;
            It it;
            unchecked_type unchecked;
        };

    public:

        using size_type = size_t;
        using iterator = view_iterator<sparse_set::const_iterator>;
        using reverse_iterator = view_iterator<sparse_set::const_reverse_iterator>;

    public:

        entity_view() : view{}
        {}

        entity_view(component_pool<Cs>&... components)
            : pools{ &components... }, view{ smallest_set() }
        {}

        explicit operator bool() const
        {
            return view != nullptr;
        }

        bool contains(entity e) const
        {
            return (std::get<component_pool<Cs>*>(pools)->contains(e) && ...);
        }

        iterator begin() const
        {
            return iterator(view->begin(), view->end(), view->begin(), get_unchecked(view));
        }

        iterator end() const
        {
            return iterator(view->begin(), view->end(), view->end(), get_unchecked(view)); //
        }

        reverse_iterator rbegin() const
        {
            return reverse_iterator(view->rbegin(), view->rend(), view->rbegin(), get_unchecked(view));
        }

        reverse_iterator rend() const {
            return reverse_iterator(view->rbegin(), view->rend(), view->rend(), get_unchecked(view));
        }

        template <typename F> requires valid_each_function<F>
        void each(F&& f)
        {
            for (auto& entity : *this) f(entity);
        }

        template<typename... _Cs> requires (sizeof...(_Cs) != 1)
        decltype(auto) get(entity e) const
        {
            static_assert(details::is_subset_of<std::tuple<std::remove_const_t<_Cs>...>, std::tuple<Cs...> >);
            assert(contains(e));

            if constexpr (sizeof...(_Cs) == 0)
                return std::forward_as_tuple(std::get<component_pool<Cs>*>(pools)->get(e)...);
            else
                return std::forward_as_tuple(const_cast<_Cs&>(std::get<component_pool<std::remove_const_t<_Cs>>*>(pools)->get(e)) ...);
        }

        template<typename C>
        decltype(auto) get(entity e) const
        {
            static_assert(details::contains<std::remove_const_t<C>, Cs...>);
            assert(contains(e));

            return (const_cast<C&>(std::get<component_pool<std::remove_const_t<C>>*>(pools)->get(e)));
        }

        template<typename... Lhs, typename... Rhs>
        friend auto operator|(entity_view<Lhs...> const&, entity_view<Rhs...> const&);

    private:
        const std::tuple<component_pool<Cs>*...> pools;
        mutable sparse_set const* view;
    };


    template<typename C>
    class entity_view<C>
    {
    public:
        using component_type = C;
        using size_type = size_t;
        using iterator = sparse_set::const_iterator;
        using reverse_iterator = sparse_set::const_reverse_iterator;

    public:

        entity_view()
            : pools{}
        {}

        entity_view(component_pool<C>& component_pool)
            : pools{ &component_pool }
        {}

        size_type size() const
        {
            return std::get<0>(pools)->size();
        }

        bool empty() const
        {
            return std::get<0>(pools)->empty();
        }

        iterator begin() const
        {
            return std::get<0>(pools)->begin();
        }

        iterator end() const
        {
            return std::get<0>(pools)->end();
        }

        reverse_iterator rbegin() const
        {
            return std::get<0>(pools)->rbegin();
        }

        reverse_iterator rend() const
        {
            return std::get<0>(pools)->rend();
        }

        template <typename F> requires valid_each_function<F>
        void each(F&& f)
        {
            for (auto& entity : *this) f(entity);
        }

        entity operator[](size_type pos) const
        {
            return begin()[pos];
        }

        explicit operator bool() const
        {
            return std::get<0>(pools) != nullptr;
        }

        bool contains(entity e) const
        {
            return std::get<0>(pools)->contains(e);
        }


        decltype(auto) get(entity e) const
        {
            assert(contains(entity));
            return const_cast<component_pool<component_type> const*>(std::get<0>(pools))->get(e);
        }

        decltype(auto) get(entity e)
        {
            assert(contains(e));
            return std::get<0>(pools)->get(e);
        }

        template<typename... Lhs, typename... Rhs>
        friend auto operator|(entity_view<Lhs...> const&, entity_view<Rhs...> const&);

    private:
        std::tuple<component_pool<component_type>*> const  pools;
    };


    template<typename... Lhs, typename... Rhs>
    auto operator|(entity_view<Lhs...> const& lhs, entity_view<Rhs...> const& rhs)
    {
        using view_type = entity_view<Lhs..., Rhs...>;
        return std::apply([](auto*... storage) { return view_type{ *storage... }; }, std::tuple_cat(
            lhs.pools, rhs.pools));
    }

}