#pragma once


namespace adria
{
	template<typename T, Uint64 BlockSize = 4096>
	class LinearAllocator
	{
	private:
		struct MemoryBlock 
		{
			Uint8* buffer;
			Uint64 used;
			Uint64 capacity;
			MemoryBlock* next;

			MemoryBlock(Uint64 size)
				: buffer(new Uint8[size]), used(0), capacity(size), next(nullptr) 
			{}

			~MemoryBlock() 
			{
				delete[] buffer;
			}
		};

		MemoryBlock* current_block;
		MemoryBlock* first_block;
		Uint64 block_size;

	public:
		using value_type = T;
		using pointer = T*;
		using const_pointer = T const*;
		using reference = T&;
		using const_reference = T const&;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

		template<typename U>
		struct rebind 
		{
			using other = LinearAllocator<U, BlockSize>;
		};

		LinearAllocator()
			: current_block(nullptr), first_block(nullptr), block_size(BlockSize) {
			first_block = new MemoryBlock(block_size);
			current_block = first_block;
		}

		~LinearAllocator() 
		{
			Free();
		}

		ADRIA_NONCOPYABLE(LinearAllocator)

		LinearAllocator(LinearAllocator&& other) noexcept
			: current_block(other.current_block),
			first_block(other.first_block),
			block_size(other.block_size) 
		{
			other.current_block = nullptr;
			other.first_block = nullptr;
		}

		T* allocate(Uint64 n) 
		{
			Uint64 bytes_needed = n * sizeof(T);

			// Check if we have enough space in the current block
			if (current_block->used + bytes_needed > current_block->capacity) 
			{
				Uint64 new_block_size = std::max(block_size, bytes_needed);
				MemoryBlock* new_block = new MemoryBlock(new_block_size);
				current_block->next = new_block;
				current_block = new_block;
			}

			void* result = current_block->buffer + current_block->used;
			current_block->used += bytes_needed;

			Uint64 alignment = alignof(T);
			Uint64 misalignment = reinterpret_cast<Uintptr>(result) % alignment;
			if (misalignment != 0) 
			{
				Uint64 padding = alignment - misalignment;
				result = static_cast<Uint8*>(result) + padding;
				current_block->used += padding;
			}
			return reinterpret_cast<T*>(result);
		}

		void deallocate(T* p, Uint64 n) noexcept 
		{
		}

		void Reset() 
		{
			MemoryBlock* block = first_block;
			while (block) 
			{
				block->used = 0;
				block = block->next;
			}
			current_block = first_block;
		}

		void Free()
		{
			MemoryBlock* block = first_block;
			while (block)
			{
				MemoryBlock* next = block->next;
				delete block;
				block = next;
			}
			first_block = nullptr;
			current_block = nullptr;
		}

		template<typename U, typename... Args>
		void construct(U* p, Args&&... args) 
		{
			new (p) U(std::forward<Args>(args)...);
		}

		template<typename U>
		void destroy(U* p) 
		{
			p->~U();
		}
	};
}