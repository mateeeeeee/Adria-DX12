#pragma once
#include "AllocatorUtil.h"

namespace adria
{
	class LinearAllocator
	{
	public:

		LinearAllocator(OffsetType max_size, OffsetType reserve = 0) noexcept :
			max_size{ max_size }, reserve{ reserve }, top{ reserve }
		{}
		ADRIA_DEFAULT_COPYABLE_MOVABLE(LinearAllocator)
		~LinearAllocator() = default;

		OffsetType Allocate(OffsetType size, OffsetType align = 0)
		{
			auto aligned_top = Align(top, align);
			if (aligned_top + size > max_size) return INVALID_OFFSET;
			else
			{
				OffsetType start = aligned_top;
				aligned_top += size;
				top = aligned_top;
				return start;
			}
		}

		void Clear()
		{
			top = reserve;
		}

		OffsetType MaxSize()  const { return max_size; }
		bool Full()			  const { return top == max_size; };
		bool Empty()		  const { return top == reserve; };
		OffsetType UsedSize() const { return top; }

	private:
		OffsetType max_size;
		OffsetType reserve;
		OffsetType top;
	};
}
