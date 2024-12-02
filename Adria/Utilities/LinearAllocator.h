#pragma once
#include "AllocatorUtil.h"

namespace adria
{
	class LinearAllocator
	{
	public:

		LinearAllocator(Uint64 max_size, Uint64 reserve = 0) noexcept :
			max_size{ max_size }, reserve{ reserve }, top{ reserve }
		{}
		ADRIA_DEFAULT_COPYABLE_MOVABLE(LinearAllocator)
		~LinearAllocator() = default;

		Uint64 Allocate(Uint64 size, Uint64 align = 0)
		{
			auto aligned_top = Align(top, align);
			if (aligned_top + size > max_size) return INVALID_ALLOC_OFFSET;
			else
			{
				Uint64 start = aligned_top;
				aligned_top += size;
				top = aligned_top;
				return start;
			}
		}
		void Clear()
		{
			top = reserve;
		}

		Uint64 MaxSize()  const { return max_size; }
		Bool Full()			  const { return top == max_size; };
		Bool Empty()		  const { return top == reserve; };
		Uint64 UsedSize() const { return top; }

	private:
		Uint64 max_size;
		Uint64 reserve;
		Uint64 top;
	};
}
