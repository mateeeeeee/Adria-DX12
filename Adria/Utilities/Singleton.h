#pragma once

namespace adria
{
	template<typename T>
	class Singleton
	{
	public:
		Singleton(Singleton const&) = delete;
		Singleton& operator=(Singleton const&) = delete;

		static T& Get()
		{
			static T instance;
			return instance;
		}

	protected:
		Singleton() {}
		~Singleton() {}
	};
}