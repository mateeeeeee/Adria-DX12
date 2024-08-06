#pragma once

namespace adria
{
	template<typename T>
	class Singleton
	{
	public:
		ADRIA_NONCOPYABLE(Singleton)

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