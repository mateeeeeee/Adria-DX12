#pragma once
#include <type_traits>

namespace adria
{
	//modified ComPtr code to avoid header inclusion
	template <typename T>
	class AutoRefCountPtr
	{
	public:
		using InterfaceType = T;

	public:
#pragma region constructors
		AutoRefCountPtr() : ptr_(nullptr)
		{
		}

		AutoRefCountPtr(decltype(nullptr)) : ptr_(nullptr)
		{
		}

		template<typename U>
		AutoRefCountPtr(U* other)  : ptr_(other)
		{
			InternalAddRef();
		}

		AutoRefCountPtr(AutoRefCountPtr const& other) : ptr_(other.ptr_)
		{
			InternalAddRef();
		}

		// copy constructor that allows to instantiate class when U* is convertible to T*
		template<typename U>
		AutoRefCountPtr(const AutoRefCountPtr<U>& other, typename std::enable_if_t<std::is_convertible_v<U*, T*>, void*>* = 0) :
			ptr_(other.ptr_)
		{
			InternalAddRef();
		}

		AutoRefCountPtr(AutoRefCountPtr&& other) : ptr_(nullptr)
		{
			if (this != reinterpret_cast<AutoRefCountPtr*>(&reinterpret_cast<unsigned char&>(other)))
			{
				Swap(other);
			}
		}

		// Move constructor that allows instantiation of a class when U* is convertible to T*
		template<typename U>
		AutoRefCountPtr(AutoRefCountPtr<U>&& other, typename std::enable_if_t<std::is_convertible_v<U*, T*>, void*>* = 0) :
			ptr_(other.ptr_)
		{
			other.ptr_ = nullptr;
		}
#pragma endregion

#pragma region destructor
		~AutoRefCountPtr() throw()
		{
			InternalRelease();
		}
#pragma endregion

#pragma region assignment
		AutoRefCountPtr& operator=(decltype(nullptr)) 
		{
			InternalRelease();
			return *this;
		}

		AutoRefCountPtr& operator=(T* other) 
		{
			if (ptr_ != other)
			{
				AutoRefCountPtr(other).Swap(*this);
			}
			return *this;
		}

		template <typename U>
		AutoRefCountPtr& operator=(U* other) 
		{
			AutoRefCountPtr(other).Swap(*this);
			return *this;
		}

		AutoRefCountPtr& operator=(const AutoRefCountPtr& other)
		{
			if (ptr_ != other.ptr_)
			{
				AutoRefCountPtr(other).Swap(*this);
			}
			return *this;
		}

		template<typename U>
		AutoRefCountPtr& operator=(const AutoRefCountPtr<U>& other) 
		{
			AutoRefCountPtr(other).Swap(*this);
			return *this;
		}

		AutoRefCountPtr& operator=(AutoRefCountPtr&& other) 
		{
			AutoRefCountPtr(static_cast<AutoRefCountPtr&&>(other)).Swap(*this);
			return *this;
		}

		template<typename U>
		AutoRefCountPtr& operator=(_Inout_ AutoRefCountPtr<U>&& other)
		{
			AutoRefCountPtr(static_cast<AutoRefCountPtr<U>&&>(other)).Swap(*this);
			return *this;
		}
#pragma endregion

#pragma region modifiers
		void Swap(AutoRefCountPtr&& r) 
		{
			T* tmp = ptr_;
			ptr_ = r.ptr_;
			r.ptr_ = tmp;
		}

		void Swap(AutoRefCountPtr& r)
		{
			T* tmp = ptr_;
			ptr_ = r.ptr_;
			r.ptr_ = tmp;
		}
#pragma endregion

		operator bool() const 
		{
			return Get() != nullptr;
		}

		operator T*() const
		{
			return Get();
		}

		T* Get() const 
		{
			return ptr_;
		}

		InterfaceType* operator->() const 
		{
			return ptr_;
		}

		T* const* GetAddressOf() const 
		{
			return &ptr_;
		}

		T** GetAddressOf() 
		{
			return &ptr_;
		}

		T** ReleaseAndGetAddressOf() 
		{
			InternalRelease();
			return &ptr_;
		}

		T* Detach() 
		{
			T* ptr = ptr_;
			ptr_ = nullptr;
			return ptr;
		}
		void Attach(InterfaceType* other) 
		{
			if (ptr_ != nullptr)
			{
				auto ref = ptr_->Release();
				(void)ref; 
				assert(ref != 0 || ptr_ != other);
			}
			ptr_ = other;
		}

		unsigned long Reset()
		{
			return InternalRelease();
		}

		// query for U interface
		template<typename U>
		HRESULT As(AutoRefCountPtr<U>* p) const 
		{
			return ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(p->ReleaseAndGetAddressOf()));
		}

	protected:
		InterfaceType* ptr_;
		template<class U> friend class AutoRefCountPtr;

		void InternalAddRef() const
		{
			if (ptr_ != nullptr)
			{
				ptr_->AddRef();
			}
		}
		unsigned long InternalRelease()
		{
			unsigned long ref = 0;
			T* temp = ptr_;

			if (temp != nullptr)
			{
				ptr_ = nullptr;
				ref = temp->Release();
			}

			return ref;
		}
	};    // AutoRefCountPtr

	template <typename T>
	using ArcPtr = AutoRefCountPtr<T>;
}