#pragma once
#include <type_traits>

namespace adria
{
	//modified ComPtr code to avoid header inclusion
	template <typename T>
	class RefCountPtr
	{
	public:
		using InterfaceType = T;

	public:
#pragma region constructors
		RefCountPtr() : ptr_(nullptr)
		{
		}

		RefCountPtr(decltype(nullptr)) : ptr_(nullptr)
		{
		}

		template<typename U>
		RefCountPtr(U* other)  : ptr_(other)
		{
			InternalAddRef();
		}

		RefCountPtr(RefCountPtr const& other) : ptr_(other.ptr_)
		{
			InternalAddRef();
		}

		// copy constructor that allows to instantiate class when U* is convertible to T*
		template<typename U>
		RefCountPtr(const RefCountPtr<U>& other, typename std::enable_if_t<std::is_convertible_v<U*, T*>, void*>* = 0) :
			ptr_(other.ptr_)
		{
			InternalAddRef();
		}

		RefCountPtr(RefCountPtr&& other) : ptr_(nullptr)
		{
			if (this != reinterpret_cast<RefCountPtr*>(&reinterpret_cast<unsigned char&>(other)))
			{
				Swap(other);
			}
		}

		// Move constructor that allows instantiation of a class when U* is convertible to T*
		template<typename U>
		RefCountPtr(RefCountPtr<U>&& other, typename std::enable_if_t<std::is_convertible_v<U*, T*>, void*>* = 0) :
			ptr_(other.ptr_)
		{
			other.ptr_ = nullptr;
		}
#pragma endregion

#pragma region destructor
		~RefCountPtr() throw()
		{
			InternalRelease();
		}
#pragma endregion

#pragma region assignment
		RefCountPtr& operator=(decltype(nullptr)) 
		{
			InternalRelease();
			return *this;
		}

		RefCountPtr& operator=(T* other) 
		{
			if (ptr_ != other)
			{
				RefCountPtr(other).Swap(*this);
			}
			return *this;
		}

		template <typename U>
		RefCountPtr& operator=(U* other) 
		{
			RefCountPtr(other).Swap(*this);
			return *this;
		}

		RefCountPtr& operator=(const RefCountPtr& other)
		{
			if (ptr_ != other.ptr_)
			{
				RefCountPtr(other).Swap(*this);
			}
			return *this;
		}

		template<typename U>
		RefCountPtr& operator=(const RefCountPtr<U>& other) 
		{
			RefCountPtr(other).Swap(*this);
			return *this;
		}

		RefCountPtr& operator=(RefCountPtr&& other) 
		{
			RefCountPtr(static_cast<RefCountPtr&&>(other)).Swap(*this);
			return *this;
		}

		template<typename U>
		RefCountPtr& operator=(_Inout_ RefCountPtr<U>&& other)
		{
			RefCountPtr(static_cast<RefCountPtr<U>&&>(other)).Swap(*this);
			return *this;
		}
#pragma endregion

#pragma region modifiers
		void Swap(RefCountPtr&& r) 
		{
			T* tmp = ptr_;
			ptr_ = r.ptr_;
			r.ptr_ = tmp;
		}

		void Swap(RefCountPtr& r)
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
		HRESULT As(RefCountPtr<U>* p) const 
		{
			return ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(p->ReleaseAndGetAddressOf()));
		}

	protected:
		InterfaceType* ptr_;
		template<class U> friend class RefCountPtr;

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
	using Ref = RefCountPtr<T>;
}