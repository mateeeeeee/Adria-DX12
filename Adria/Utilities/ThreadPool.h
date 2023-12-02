#pragma once
#include <thread>
#include <future>
#include <type_traits>
#include "ConcurrentQueue.h"
#include "Singleton.h"

namespace adria
{
	class ThreadPool : public Singleton<ThreadPool>
	{
		friend class Singleton<ThreadPool>;
	public:

		ADRIA_NONCOPYABLE_NONMOVABLE(ThreadPool);
		~ThreadPool() = default;

		void Initialize(uint32 pool_size = std::thread::hardware_concurrency() - 1)
		{
			done = false;
			static const uint32 max_threads = std::thread::hardware_concurrency();
			uint16 const num_threads = pool_size == 0 ? max_threads - 1 : (std::min)(max_threads - 1, pool_size);

			threads.reserve(num_threads);
			for (uint16 i = 0; i < num_threads; ++i)
			{
				threads.emplace_back(std::bind(&ThreadPool::ThreadWork, this));
			}
		}
		void Destroy()
		{
			if (done) return;
			{
				std::unique_lock<std::mutex> lk(cond_mutex);
				done = true;
				cond_var.notify_all();
			}
			for (uint16 i = 0; i < threads.size(); ++i) if (threads[i].joinable())  threads[i].join();
		}

		template<typename F, typename... Args>
		auto Submit(F&& f, Args&&... args) 
		{
			using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
			auto bind_f = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
			auto wrapped_task = std::make_shared<std::packaged_task<ReturnType()>>(bind_f);
			std::future<ReturnType> result_future = wrapped_task->get_future();
			auto void_task = [wrapped_task]() {(*wrapped_task)(); };
			task_queue.Push(std::move(void_task));
			cond_var.notify_one();
			return result_future;
		}

	private:
		std::vector<std::thread> threads;
		ConcurrentQueue<std::function<void()>> task_queue;
		bool done = false;
		std::condition_variable cond_var;
		std::mutex cond_mutex;

	private:
		ThreadPool() = default;

		void ThreadWork()
		{
			std::function <void()> task;
			bool pop_success;

			while (true)
			{
				{
					std::unique_lock<std::mutex> lk(cond_mutex);
					while (!done && task_queue.Empty()) cond_var.wait(lk);
					if (done) return;
					else pop_success = task_queue.TryPop(task);
				}

				if (pop_success)
				{
					task();
					task = nullptr;
				}
			}
		}
	};
	#define g_ThreadPool ThreadPool::Get()
}