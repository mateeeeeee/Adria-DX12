#pragma once
#include "Task.h"
#include "ThreadPool.h"
#include "Utilities/Singleton.h"

namespace adria
{

	class TaskManager : public Singleton<TaskManager>
	{
		friend class Singleton<TaskManager>;
	public:

		void Initialize(uint32_t thread_count = 0)
		{
			thread_pool = std::make_unique<ThreadPool>(thread_count);
		}
		void Destroy()
		{
			thread_pool.reset();
		}

		template<typename F, typename... Args>
		std::shared_ptr<Task> CreateTask(F&& f, Args&&... args)
		{
			auto taskfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
			std::shared_ptr<Task> task = std::make_shared<Task>(taskfunc);
			return task;
		}
		void DestroyTask(std::shared_ptr<Task>& task)
		{
			task.reset();
		}
		auto SubmitTask(std::shared_ptr<Task>& task)
		{
			return thread_pool->Submit(&Task::Run, task);
		}

	private:
		std::unique_ptr<ThreadPool> thread_pool;

	private:
		TaskManager() = default;
	};
	#define g_TaskManager TaskManager::Get()
}