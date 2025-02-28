#pragma once
#include <vector>

namespace adria
{
	class RenderGraph;
	class RenderGraphAllocator
	{
		friend class RenderGraph;

		struct AllocatedObject
		{
			virtual ~AllocatedObject() = default;
		};
		template<typename T> requires (!std::is_trivial_v<T>)
		struct NonTrivialAllocatedObject : public AllocatedObject
		{
			template<typename... Args>
			NonTrivialAllocatedObject(Args&&... args) : object(std::forward<Args>(args)...)
			{
			}

			T object;
		};
	public:
		RenderGraphAllocator(Uint64 size)
			: capacity(size), data(new Uint8[size]), current_offset(data)
		{
		}
		~RenderGraphAllocator()
		{
			for (Uint32 i = 0; i < non_trivial_object_allocations.size(); ++i)
			{
				non_trivial_object_allocations[i]->~AllocatedObject();
			}
			delete[] data;
		}

		template<typename T, typename ...Args>
		ADRIA_NODISCARD T* AllocateObject(Args&&... args)
		{
			using AllocatedType = std::conditional_t<std::is_trivial_v<T>, T, NonTrivialAllocatedObject<T>>;
			void* alloc = Allocate(sizeof(AllocatedType));
			AllocatedType* allocation = new (alloc) AllocatedType(std::forward<Args>(args)...);

			if constexpr (std::is_trivial_v<T>)
			{
				return allocation;
			}
			else
			{
				non_trivial_object_allocations.push_back(allocation);
				return &allocation->object;
			}
		}

		ADRIA_NODISCARD void* Allocate(Uint64 size)
		{
			ADRIA_ASSERT(current_offset - data + size < capacity);
			void* current_data = current_offset;
			current_offset += size;
			return current_data;
		}

		Uint64 GetSize() const { return static_cast<Uint64>(current_offset - data); }
		Uint64 GetCapacity() const { return capacity; }

	private:
		std::vector<AllocatedObject*> non_trivial_object_allocations;
		Uint64 capacity;
		Uint8* data;
		Uint8* current_offset;
	};
	using RGAllocator = RenderGraphAllocator;
}
