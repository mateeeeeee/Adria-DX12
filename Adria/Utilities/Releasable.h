#pragma once

#include <concepts>
#include <memory>

namespace adria
{
    template<typename T>
    concept Releasable = requires (T t)
    {
        {t.Release()};
    };

    struct ReleaseDeleter
    {
        template<Releasable T>
        void operator()(T* allocation)
        {
            allocation->Release();
        }
    };

    template<Releasable T>
    using ReleasablePtr = std::unique_ptr<T, ReleaseDeleter>;

    struct ReleasableObject
    {
		virtual void Release() = 0;
        virtual ~ReleasableObject() = default;
    };

    template<Releasable T>
    struct ReleasableResource : ReleasableObject
    {
        ReleasableResource(T* r)
        { 
            resource = r;
        }

        virtual ~ReleasableResource()
        {
            Release();
        }

        virtual void Release() override
        {
            if(resource) resource->Release();
        }

        T* resource;
    };

}