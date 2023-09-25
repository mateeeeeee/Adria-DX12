#pragma once
#include <memory>
#include <vector>
#include <cassert>

namespace adria
{
	template<typename T>
	class RingBuffer
	{
	public:
		using value_type = T;
		using pointer = T*;
		using const_pointer = T const*;
		using reference = T&;
		using const_reference = T const&;
		using size_type = size_t;
		using difference_type = ptrdiff_t;

		class RingBufferIterator
		{
		public:
			friend class RingBuffer;
			friend class ConstRingBufferIterator;

			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = T;
			using difference_type = ptrdiff_t;
			using pointer = T*;
			using reference = T&;

		private:
			RingBufferIterator(
				RingBuffer& cbuf, size_t start_pos)
				: buf(cbuf), pos(start_pos) {}

		public:
			RingBufferIterator(RingBufferIterator const&) = default;
			RingBufferIterator(RingBufferIterator&&) = default;
			~RingBufferIterator() = default;

			RingBufferIterator& operator=(RingBufferIterator const&) = default;
			RingBufferIterator& operator=(RingBufferIterator&&) = default;

			reference operator*()
			{
				return buf.buffer[pos];
			}

			pointer operator->()
			{
				return &(operator*());
			}

			RingBufferIterator& operator++()
			{

				++pos;

				pos = pos % buf.buffer.size();

				return *this;
			}

			RingBufferIterator operator++(int)
			{
				RingBufferIterator tmp(*this);

				operator++();

				return tmp;
			}

			RingBufferIterator& operator--()
			{

				pos = (pos - 1 + buf.buffer.size()) % buf.buffer.size();

				return *this;
			}

			RingBufferIterator operator--(int)
			{
				RingBufferIterator tmp(*this);

				operator--();

				return tmp;
			}

			RingBufferIterator operator+(difference_type n)
			{
				RingBufferIterator tmp(*this);
				tmp.pos += n;
				tmp.pos %= buf.buffer.size();
				return tmp;
			}

			RingBufferIterator operator-(difference_type n)
			{
				RingBufferIterator tmp(*this);
				tmp.pos = (tmp.pos + buf.buffer.size() * n - n) % buf.buffer.size();
				return tmp;
			}

			RingBufferIterator& operator+=(difference_type n)
			{
				pos += n;
				pos %= buf.buffer.size();
				return *this;
			}

			RingBufferIterator& operator-=(difference_type n)
			{
				pos = (pos + buf.buffer.size() * n - n) % buf.buffer.size();
				return *this;
			}

			bool operator==(RingBufferIterator const& other) const
			{
				return buf.buffer.data() == other.buf.buffer.data() && pos == other.pos;
			}

			bool operator!=(RingBufferIterator const& other) const
			{
				return !(*this == other);
			}


		private:
			RingBuffer& buf;
			size_t pos;
		};

		class ConstRingBufferIterator
		{
		public:
			friend class RingBuffer;

			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = T;
			using difference_type = ptrdiff_t;
			using pointer = T  const*;
			using reference = T const&;


		private:
			ConstRingBufferIterator(RingBuffer const& cbuf, size_t start_pos) : buf(cbuf), pos(start_pos) {}

		public:
			ConstRingBufferIterator() = default;

			ConstRingBufferIterator(RingBufferIterator const& o) : buf{ o.buf }, pos{ o.pos }{}
			ConstRingBufferIterator(ConstRingBufferIterator const&) = default;
			ConstRingBufferIterator(ConstRingBufferIterator&&) = default;
			~ConstRingBufferIterator() = default;

			ConstRingBufferIterator& operator=(ConstRingBufferIterator const&) = default;
			ConstRingBufferIterator& operator=(ConstRingBufferIterator&&) = default;

			reference operator*()
			{
				return buf.buffer[pos];
			}

			pointer operator->()
			{
				return &(operator*());
			}

			ConstRingBufferIterator& operator++()
			{
				++pos;

				pos = pos % buf.buffer.size();

				return *this;
			}

			ConstRingBufferIterator operator++(int)
			{
				ConstRingBufferIterator tmp(*this);

				operator++();

				return tmp;
			}

			ConstRingBufferIterator& operator--()
			{

				pos = (pos - 1 + buf.buffer.size()) % buf.buffer.size();

				return *this;
			}

			ConstRingBufferIterator operator--(int)
			{
				ConstRingBufferIterator tmp(*this);

				operator--();

				return tmp;
			}

			ConstRingBufferIterator operator+(difference_type n)
			{
				ConstRingBufferIterator tmp(*this);
				tmp.pos += n;
				tmp.pos %= buf.buffer.size();
				return tmp;
			}

			ConstRingBufferIterator operator-(difference_type n)
			{
				ConstRingBufferIterator tmp(*this);
				tmp.pos = (tmp.pos + buf.buffer.size() * n - n) % buf.buffer.size();
				return tmp;
			}

			ConstRingBufferIterator& operator+=(difference_type n)
			{
				pos += n;
				pos %= buf.Capacity();
				return *this;
			}

			ConstRingBufferIterator& operator-=(difference_type n)
			{
				pos = (pos + buf.buffer.size() * n - n) % buf.buffer.size();
				return *this;
			}

			bool operator==(ConstRingBufferIterator const& other) const
			{
				return buf.buffer.data() == other.buf.buffer.data() && pos == other.pos;
			}

			bool operator!=(ConstRingBufferIterator const& other) const
			{
				return !(*this == other);
			}


		private:
			RingBuffer const& buf;
			size_t pos;

		private:
		};

		using iterator = RingBufferIterator;
		using const_iterator = ConstRingBufferIterator;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	public:
		RingBuffer() = delete;
		explicit RingBuffer(size_type buffer_capacity) : buffer(buffer_capacity + 1), head{ 0 }, tail{ 0 }, content_size{ 0 } {}
		RingBuffer(RingBuffer const&) = default;
		RingBuffer(RingBuffer&&) = default;
		RingBuffer& operator=(RingBuffer const&) = default;
		RingBuffer& operator=(RingBuffer&&) = default;
		~RingBuffer() = default;

		iterator begin()
		{
			return iterator(*this, head);
		}

		const_iterator begin() const
		{
			return const_iterator(*this, head);
		}

		const_iterator cbegin() const
		{
			return const_iterator(*this, head);
		}

		iterator end()
		{
			return iterator(*this, tail);
		}

		const_iterator end() const
		{
			return const_iterator(*this, tail);
		}

		const_iterator cend() const
		{
			return const_iterator(*this, tail);
		}

		reverse_iterator rbegin()
		{
			return reverse_iterator(end());
		}

		const_reverse_iterator rbegin() const
		{
			return const_reverse_iterator(end());
		}

		const_reverse_iterator crbegin() const
		{
			return const_reverse_iterator(end());
		}

		reverse_iterator rend()
		{
			return reverse_iterator(begin());
		}

		const_reverse_iterator rend() const
		{
			return const_reverse_iterator(begin());
		}

		const_reverse_iterator crend() const
		{
			return const_reverse_iterator(begin());
		}

		size_type Size() const
		{
			return content_size;
		}

		size_type Capacity() const
		{
			return buffer.size() - 1;
		}

		bool Empty() const
		{
			return content_size == 0;
		}

		bool Full() const
		{
			return content_size == Capacity();
		}

		void PushBack(const_reference item)
		{

			size_type item_index = tail;
			buffer[item_index] = item;
			if (Full())
				HeadIncrement();
			TailIncrement();

		}

		void PopFront() { HeadIncrement(); }

		reference Front()
		{
			return *begin();
		}
		const_reference Front() const
		{
			return *cbegin();
		}
		reference Back()
		{
			return *(--end());
		}
		const_reference Back() const
		{
			return *(--cend());
		}

		reference operator[](size_type i)
		{
			iterator it = begin();
			it += i;
			return *it;
		}

		const_reference operator[](size_type i) const
		{
			iterator it = cbegin();
			it += i;
			return *it;
		}

		reference At(size_type i)
		{
			assert(InBounds(i) && "Out of Bounds Index");
			iterator it = begin();
			it += i;
			return *it;
		}

		const_reference At(size_type i) const
		{
			assert(InBounds(i) && "Out of Bounds Index");
			iterator it = cbegin();
			it += i;
			return *it;
		}

		void Clear()
		{
			head = tail = content_size = 0;
		}


	private:
		std::vector<value_type> buffer;
		size_type head, tail;
		size_type content_size;

	private:

		void TailIncrement()
		{
			++tail;
			++content_size;
			if (tail == buffer.size()) tail = 0;
		}

		void HeadIncrement()
		{
			assert(!Empty() && "Cannot call Pop Front on empty buffer");
			++head;
			--content_size;
			if (head == buffer.size()) head = 0;
		}

		bool InBounds(size_type i) const
		{
			return (i >= head && i <= tail) || !(i > tail && i < head);
		}

	};
}