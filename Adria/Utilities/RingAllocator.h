#pragma once
#include <queue>
#include "AllocatorUtil.h"

namespace adria
{
	class RingAllocator
	{
	public:

		struct BufferEntry
		{
			BufferEntry(uint64 fv, OffsetType off, OffsetType sz) :
				frame(fv),
				offset(off),
				size(sz)
			{}
			uint64 frame;
			OffsetType offset;
			OffsetType size;
		};

	public:

		RingAllocator(OffsetType max_size, OffsetType reserve = 0) noexcept :
			completed_frames{},
			reserve{ reserve },
			max_size{ max_size - reserve },
			head{ 0 },
			tail{ 0 },
			used_size{ reserve }
		{}

		ADRIA_DEFAULT_COPYABLE_MOVABLE(RingAllocator)
		~RingAllocator() = default;

		OffsetType Allocate(OffsetType size, OffsetType align = 0)
		{
			if (Full()) return INVALID_OFFSET;

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
					OffsetType add_size = (max_size - tail) + size;
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

			return INVALID_OFFSET;
		}

		void FinishCurrentFrame(uint64 frame)
		{
			completed_frames.emplace(frame, tail, current_frame_size);
			current_frame_size = 0;
		}

		void ReleaseCompletedFrames(uint64 completed_frame)
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

		OffsetType MaxSize()  const { return max_size; }
		bool Full()			  const { return used_size == max_size; };
		bool Empty()		  const { return used_size == reserve; };
		OffsetType UsedSize() const { return used_size; }

	private:
		std::queue<BufferEntry> completed_frames;
		OffsetType reserve = 0;
		OffsetType head = 0;
		OffsetType tail = 0;
		OffsetType max_size = 0;
		OffsetType used_size = 0;
		OffsetType current_frame_size = 0;
	};

}