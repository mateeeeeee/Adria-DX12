#pragma once
#include "Task.h"

#include "ThreadPool.h"
#include "../Utilities/ObjectPool.h"

#include <iostream>

namespace adria
{

	class TaskManager
	{
		using TaskPool = ObjectPool<Task>;

	public:

		explicit TaskManager(size_t task_count) : task_pool{ task_count }, thread_pool{} {}

		TaskManager(size_t task_count, size_t thread_count) : task_pool{ task_count }, thread_pool{ thread_count } {}

		template<typename F, typename... Args>
		std::shared_ptr<Task> CreateTask(F&& f, Args&&... args)
		{
			auto taskfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

			std::shared_ptr<Task> task(task_pool.Construct<Task>(taskfunc), [this](Task* task) {task_pool.Destroy<Task>(task); });

			return task;
		}

		void DestroyTask(std::shared_ptr<Task> task)
		{
			task_pool.Destroy<Task>(task.get());
		}

		auto SubmitTask(std::shared_ptr<Task> task)
		{
			return thread_pool.Submit(&Task::Run, task);
		}

	private:
		ThreadPool thread_pool;
		TaskPool task_pool;
	};

}