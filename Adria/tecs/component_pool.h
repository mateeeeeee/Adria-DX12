#pragma once
#include "sparse_set.h"

namespace adria::tecs
{

template <typename C, typename F>
concept component_updater = requires(C& c, F && f) { { f(c) } ->std::same_as<void>; };

template<typename C> 
class component_pool : public sparse_set
{
    using base_type = sparse_set;
    using component_type = C;
    using size_type = base_type::size_type;

public:
    virtual void remove(entity e) override
    {
        using std::swap;
        auto index = base_type::index(e);
        swap(components[index], components.back());
        components.pop_back();
        base_type::remove(e);
    }

    virtual void clear() override
    {
        components.clear();
        base_type::clear();
    }

    size_type size() const
    {
        return components.size();
    }

    component_type const& get(entity e) const
    {
        return components[base_type::index(e)];
    }

    component_type& get(entity e)
    {
        return components[base_type::index(e)];
    }

    component_type const* get_if(entity e) const
    {
        if(contains(e)) return &components[base_type::index(e)];
        return nullptr;
    }

    component_type* get_if(entity e)
    {
        if (contains(e)) return &components[base_type::index(e)];
        return nullptr;
    }

    template<typename... Args>
    void emplace(entity e, Args&&... args)
    {
        assert(!contains(e));
        if constexpr (std::is_aggregate_v<component_type>)  components.push_back(component_type{ std::forward<Args>(args)... });
        else components.emplace_back(std::forward<Args>(args)...);
        base_type::emplace(e);
    }

    void add(entity e, component_type const& c)
    {
        assert(!contains(e));
        components.push_back(c);
        base_type::emplace(e);
    }

    template<typename... Args>
    void replace(entity e, Args&&... args)
    {
        assert(contains(e));
        auto&& component = components[base_type::index(e)];
        component = component_type(std::forward<Args>(args)...);
    }

    void replace(entity e, component_type const& c)
    {
        assert(contains(e));
        auto&& component = components[base_type::index(e)];
        component = c;
    }

    template<typename... F> requires (component_updater<component_type, F> && ...)
    decltype(auto) update(entity e, F&&... func)
    {
        auto&& component = components[base_type::index(e)];
        (std::forward<F>(func)(component), ...);
        return component;
    }


private:
    std::vector<component_type> components;
};

}