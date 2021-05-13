#pragma once

#include <memory>
#include <cassert>



template<typename T>
class ObjectPool
{

    union Chunk
    {
        using StorageType = char[sizeof(T)];

        Chunk* next = nullptr;

        StorageType object;
    };


public:
    ObjectPool(size_t size) : pool_size{ size }, used{ 0 }
    {

        mem_alloc = reinterpret_cast<Chunk*>(malloc(sizeof(Chunk)));

        Chunk* tmp = mem_alloc;

        for (size_t i = 0; i < size - 1; ++i)
        {
            tmp->next = reinterpret_cast<Chunk*>(malloc(sizeof(Chunk)));

            tmp = tmp->next;
        }
    }

    ObjectPool(ObjectPool const&) = delete;
    ObjectPool(ObjectPool&&) = delete;

    ~ObjectPool() = default;

    [[nodiscard]] void* Allocate()
    {
        if (mem_alloc == nullptr)
            return nullptr;

        Chunk* free_chunk = mem_alloc;

        mem_alloc = mem_alloc->next;

        used += sizeof(Chunk);

        return free_chunk;
    }

    void Free(void* chunk)
    {

        Chunk* new_start_ptr = reinterpret_cast<Chunk*>(chunk);

        new_start_ptr->next = mem_alloc;

        mem_alloc = new_start_ptr;

        used -= sizeof(Chunk);
    }

    template<typename T, typename ...Args>
    T* Construct(Args&& ...args)
    {
        return new (Allocate()) T(std::forward<Args>(args)...);
    }

    template<typename T>
    void Destroy(T* p) noexcept
    {
        if (p == nullptr) return;
            
        Free(p);

        p->~T();
    }

    size_t GetMemorySize() const
    {
        return pool_size * sizeof(Chunk);
    }

    size_t GetUsedMemorySize() const
    {
        return used;
    }

private:
    Chunk* mem_alloc = nullptr;
    size_t const pool_size;
    size_t used;

};




