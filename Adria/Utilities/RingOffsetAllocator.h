#pragma once
#include <queue>
#include "AllocatorUtil.h"

namespace adria
{
	class RingOffsetAllocator
	{
	public:

		struct BufferEntry
		{
			BufferEntry(Uint64 fv, Uint64 off, Uint64 sz) :
				frame(fv),
				offset(off),
				size(sz)
			{}
			Uint64 frame;
			Uint64 offset;
			Uint64 size;
		};

	public:

		RingOffsetAllocator(Uint64 max_size, Uint64 reserve = 0) noexcept :
			completed_frames{},
			reserve{ reserve },
			max_size{ max_size - reserve },
			head{ 0 },
			tail{ 0 },
			used_size{ reserve }
		{}

		ADRIA_DEFAULT_COPYABLE_MOVABLE(RingOffsetAllocator)
		~RingOffsetAllocator() = default;

		Uint64 Allocate(Uint64 size, Uint64 align = 0)
		{
			if (Full()) return INVALID_ALLOC_OFFSET;

			if (tail >= head)
			{
				if (tail + size <= max_size)
				{
					auto offset = tail;
					tail += size;
					used_size += size;
					current_frame_size += size;
					return offset + reserve;
				}
				else if (size <= head)
				{
					// Allocate from the beginning of the buffer
					Uint64 add_size = (max_size - tail) + size;
					used_size += add_size;
					current_frame_size += add_size;
					tail = size;
					return 0 + reserve;
				}
			}
			else if (tail + size <= head)
			{
				auto offset = tail;
				tail += size;
				used_size += size;
				current_frame_size += size;
				return offset + reserve;
			}

			return INVALID_ALLOC_OFFSET;
		}

		void FinishCurrentFrame(Uint64 frame)
		{
			completed_frames.emplace(frame, tail, current_frame_size);
			current_frame_size = 0;
		}

		void ReleaseCompletedFrames(Uint64 completed_frame)
		{
			while (!completed_frames.empty() &&
				completed_frames.front().frame <= completed_frame)
			{
				auto const& oldest_buffer_entry = completed_frames.front();
				assert(oldest_buffer_entry.size <= used_size);
				used_size -= oldest_buffer_entry.size;
				head = oldest_buffer_entry.offset;
				completed_frames.pop();
			}
		}

		Uint64 MaxSize()  const { return max_size; }
		Bool Full()			  const { return used_size == max_size; };
		Bool Empty()		  const { return used_size == reserve; };
		Uint64 UsedSize() const { return used_size; }

	private:
		std::queue<BufferEntry> completed_frames;
		Uint64 reserve = 0;
		Uint64 head = 0;
		Uint64 tail = 0;
		Uint64 max_size = 0;
		Uint64 used_size = 0;
		Uint64 current_frame_size = 0;
	};

}