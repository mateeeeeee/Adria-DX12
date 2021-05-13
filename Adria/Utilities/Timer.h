#pragma once
#include <chrono>

namespace adria
{


	template<typename Duration = std::chrono::microseconds, typename Clock = std::chrono::steady_clock>
	class Timer
	{
		using tp = typename Clock::time_point;
		
	public:

		Timer() : t0{ Clock::now() } { t1 = t0; }

		typename Duration::rep Mark()
		{
			tp t2 = Clock::now();
			auto dt = std::chrono::duration_cast<Duration>(t2 - t1).count();
			t1 = t2;
			return dt;
		}

		typename Duration::rep Peek() const
		{
			tp t2 = Clock::now();
			auto dt = std::chrono::duration_cast<Duration>(t2 - t1).count();
			return dt;
		}

		typename Duration::rep Elapsed() const
		{
			tp t2 = Clock::now();
			auto elapsed = std::chrono::duration_cast<Duration>(t2 - t0).count();
			return elapsed;
		}

		float ElapsedInSeconds() const
		{
			return Elapsed() / DurationSecondRatio();
		}

		float PeekInSeconds() const
		{

			return Peek() / DurationSecondRatio();
		}

		float MarkInSeconds()
		{

			return Mark() / DurationSecondRatio();
		}


	private:

		const tp t0;  //vrijeme stvaranja objekta
		tp t1;  //vrijeme koje se moze promijeniti sa Mark metodom

	private:

		static float DurationSecondRatio()
		{
			static const std::chrono::seconds oneSecond(1);
			static const Duration oneUnit(1);
			static const float ratio = oneSecond * 1.0f / oneUnit;
			return ratio;
		}

	};

	using EngineTimer = Timer<>;


};
