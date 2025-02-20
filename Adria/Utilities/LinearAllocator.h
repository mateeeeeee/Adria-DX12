#pragma once


namespace adria
{
	template<Uint64 BlockSize>
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

		void* Allocate(Uint64 size, Uint64 align) 
		{
			Uint64 bytes_needed = size;

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

			Uint64 alignment = align; // alignof(T);
			Uint64 misalignment = reinterpret_cast<Uintptr>(result) % alignment;
			if (misalignment != 0) 
			{
				Uint64 padding = alignment - misalignment;
				result = static_cast<Uint8*>(result) + padding;
				current_block->used += padding;
			}
			return result;
		}

		template<typename T>
		T* Allocate(Uint64 n)
		{
			return static_cast<T*>(Allocate(n * sizeof(T), alignof(T)));
		}

		void Deallocate(void* p, Uint64 n) noexcept 
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
		void Construct(U* p, Args&&... args) 
		{
			new (p) U(std::forward<Args>(args)...);
		}
		template<typename U>
		void Destroy(U* p) 
		{
			p->~U();
		}
	};
}