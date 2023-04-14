#pragma once
#include <memory>
#include <cassert>

namespace adria
{

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
        explicit ObjectPool(size_t size) : pool_size{ size * sizeof(Chunk) }, used{0}
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
        ~ObjectPool()
        {
            Chunk* curr_alloc = mem_alloc;
            while (curr_alloc != nullptr)
            {
                Chunk* temp = curr_alloc;
                curr_alloc = curr_alloc->next;
                free(temp);
            }
            mem_alloc = nullptr;
        }

        [[nodiscard]] void* Allocate()
        {
            if (mem_alloc == nullptr) return nullptr;
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
        [[nodiscard]] T* Construct(Args&& ...args)
        {
            return new (Allocate()) T(std::forward<Args>(args)...);
        }
        template<typename T>
        void Destroy(T* p) noexcept
        {
            if (p == nullptr) return;
            p->~T();
            Free(p);
        }

        size_t GetMemorySize() const
        {
            return pool_size;
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

}




