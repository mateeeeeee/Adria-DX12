#pragma once
#include <functional>
#include <vector>
#ifdef __cpp_concepts
#include <concepts>
#else 
#include <type_traits>
#endif

namespace adria
{
#define DECLARE_DELEGATE(name, ...) \
	using name = Delegate<void(__VA_ARGS__)>

#define DECLARE_DELEGATE_RETVAL(name, retval, ...) \
	using name = Delegate<retval(__VA_ARGS__)>

#define DECLARE_MULTICAST_DELEGATE(name, ...) \
	using name = MultiCastDelegate<__VA_ARGS__>; \

#define DECLARE_EVENT(name, owner, ...) \
	class name : public MultiCastDelegate<__VA_ARGS__> \
	{ \
	private: \
		friend class owner; \
		using MultiCastDelegate<__VA_ARGS__>::Broadcast; \
		using MultiCastDelegate<__VA_ARGS__>::RemoveAll; \
		using MultiCastDelegate<__VA_ARGS__>::Remove; \
	};

	template<typename...>
	class Delegate;

	template<typename R, typename... Args>
	class Delegate<R(Args...)>
	{
		using DelegateType = std::function<R(Args...)>;

	public:
		Delegate() = default;
		Delegate(Delegate const&) = default;
		Delegate(Delegate&&) noexcept = default;
		~Delegate() = default;
		Delegate& operator=(Delegate const&) = default;
		Delegate& operator=(Delegate&&) noexcept = default;

#ifdef __cpp_concepts
		template<typename F> requires std::is_constructible_v<DelegateType, F>
#else 
		template<typename F, std::enable_if_t<std::is_constructible_v<DelegateType, F>>* = nullptr>
#endif
		void Bind(F&& callable)
		{
			callback = callable;
		}

		template<typename T>
		void BindMember(R(T::* mem_pfn)(Args...), T& instance)
		{
			callback = [&instance, mem_pfn](Args&&... args) mutable -> R {return (instance.*mem_pfn)(std::forward<Args>(args)...); };
		}

		void UnBind()
		{
			callback = nullptr;
		}

		R Execute(Args&&... args)
		{
			return callback(std::forward<Args>(args)...);
		}

		bool IsBound() const { return callback != nullptr; }

	private:
		DelegateType callback = nullptr;
	};

	class DelegateHandle
	{
	public:
		DelegateHandle() : id(INVALID_ID) {}
		explicit DelegateHandle(int) : id(GenerateID()) {}
		~DelegateHandle() noexcept = default;
		DelegateHandle(DelegateHandle const&) = default;
		DelegateHandle(DelegateHandle&& that) noexcept : id(that.id)
		{
			that.Reset();
		}
		DelegateHandle& operator=(DelegateHandle const&) = default;
		DelegateHandle& operator=(DelegateHandle&& that) noexcept
		{
			id = that.id;
			that.Reset();
			return *this;
		}

		operator bool() const
		{
			return IsValid();
		}

		bool operator==(DelegateHandle const& that) const
		{
			return id == that.id;
		}

		bool operator<(DelegateHandle const& that) const
		{
			return id < that.id;
		}

		bool IsValid() const
		{
			return id != INVALID_ID;
		}

		void Reset()
		{
			id = INVALID_ID;
		}

	private:
		uint64 id;

	private:

		inline static constexpr uint64 INVALID_ID = uint64(-1);
		static uint64 GenerateID()
		{
			static uint64 current_id = 0;
			return current_id++;
		}
	};

	template<typename... Args>
	class MultiCastDelegate
	{
		using DelegateType = std::function<void(Args...)>;
		using HandleDelegatePair = std::pair<DelegateHandle, DelegateType>;

	public:
		MultiCastDelegate() = default;
		MultiCastDelegate(MultiCastDelegate const&) = default;
		MultiCastDelegate(MultiCastDelegate&&) noexcept = default;
		~MultiCastDelegate() = default;
		MultiCastDelegate& operator=(MultiCastDelegate const&) = default;
		MultiCastDelegate& operator=(MultiCastDelegate&&) noexcept = default;

#ifdef __cpp_concepts
		template<typename F> requires std::is_constructible_v<DelegateType, F>
#else 
		template<typename F, std::enable_if_t<std::is_constructible_v<DelegateType, F>>* = nullptr>
#endif
		[[maybe_unused]] DelegateHandle Add(F&& callable)
		{
			delegate_array.emplace_back(DelegateHandle(0), std::forward<F>(callable));
			return delegate_array.back().first;
		}

		template<typename T>
		[[maybe_unused]] DelegateHandle AddMember(void(T::* mem_pfn)(Args...), T& instance)
		{
			delegate_array.emplace_back(DelegateHandle(0), [&instance, mem_pfn](Args&&... args) mutable -> void {return (instance.*mem_pfn)(std::forward<Args>(args)...); });
			return delegate_array.back().first;
		}

#ifdef __cpp_concepts
		template<typename F> requires std::is_constructible_v<DelegateType, F>
#else 
		template<typename F, std::enable_if_t<std::is_constructible_v<DelegateType, F>>* = nullptr>
#endif
		[[maybe_unused]] DelegateHandle operator+=(F&& callable) noexcept
		{
			return Add(std::forward<F>(callable));
		}

		[[maybe_unused]] bool operator-=(DelegateHandle& handle)
		{
			return Remove(handle);
		}

		[[maybe_unused]] bool Remove(DelegateHandle& handle)
		{
			if (handle.IsValid())
			{
				for (uint64 i = 0; i < delegate_array.size(); ++i)
				{
					if (delegate_array[i].first == handle)
					{
						std::swap(delegate_array[i], delegate_array.back());
						delegate_array.pop_back();
						handle.Reset();
						return true;
					}
				}
			}
			return false;
		}

		void RemoveAll()
		{
			delegate_array.clear();
		}

		void Broadcast(Args... args)
		{
			for (uint64 i = 0; i < delegate_array.size(); ++i)
			{
				if (delegate_array[i].first.IsValid()) delegate_array[i].second(std::forward<Args>(args)...);
			}
		}

		bool IsHandleBound(DelegateHandle const& handle) const
		{
			if (handle.IsValid())
			{
				for (uint64 i = 0; i < delegate_array.size(); ++i)
				{
					if (delegate_array[i].Handle == handle) return true;
				}
			}
			return false;
		}

	private:
		std::vector<HandleDelegatePair> delegate_array;
	};

}