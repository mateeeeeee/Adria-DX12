#pragma once
#include "ThreadPool.h"
#include "Task.h"

namespace adria
{

	class TaskSystem
	{
	public:

		static void Initialize(uint32_t thread_count = 0)
		{
			if (!thread_count) thread_pool = std::make_unique<ThreadPool>();
			else thread_pool = std::make_unique<ThreadPool>(thread_count);
		}

		static void Destroy()
		{
			thread_pool->Destroy();
		}

		template<typename F, typename... Args>
		static auto Submit(F&& f, Args&&... args)
		{
			return thread_pool->Submit(std::forward<F>(f), std::forward<Args>(args)...);
		}

		static auto SubmitTask(std::shared_ptr<Task> task)
		{
			return thread_pool->Submit(&Task::Run, task);
		}

	private:
		static inline std::unique_ptr<ThreadPool> thread_pool = nullptr;
	};

}