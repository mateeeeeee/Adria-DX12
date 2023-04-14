#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <future>

namespace adria
{

	//using promise and future
	class Task : public std::enable_shared_from_this<Task>
	{
	public:

		Task() : taskfunc{ nullptr }, parents{}, children{}, finished{ false }
		{
			shared_future = promise.get_future().share();
		}
		Task(void(*_taskfunc)())
			: taskfunc{ _taskfunc }, parents{}, children{}, finished{ false }
		{
			shared_future = promise.get_future().share();
		}
		Task(std::function<void()>&& _taskfunc) : taskfunc{ std::move(_taskfunc) }, parents{}, children{}, finished{false}
		{
			shared_future = promise.get_future().share();
		}
		template<typename F, typename... Args>
		Task(F&& f, Args&&... args) : taskfunc{ nullptr }, parents{}, children{}, finished{ false }
		{
			taskfunc = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
			shared_future = promise.get_future().share();
		}
		Task(Task const&) = delete;
		Task(Task&& other) noexcept
			: taskfunc{ std::move(other.taskfunc) }, parents{ std::move(parents) },
			children{ std::move(other.children) }, finished{ false }, promise{ std::move(other.promise) },
			shared_future{ std::move(other.shared_future) }
		{}
		Task& operator=(Task const&) = delete;
		Task& operator=(Task&& other) noexcept
		{
			if (this == &other) return *this;

			std::swap(taskfunc, other.taskfunc);
			std::swap(parents, other.parents);
			std::swap(children, other.children);
			finished = other.finished.load();
			std::swap(shared_future, other.shared_future);
			std::swap(promise, other.promise);
			return *this;
		}
		~Task() = default;

		void AddParent(std::shared_ptr<Task> parent)
		{
			parents.push_back(parent);

			parent->children.push_back(Shared());
		}
		void AddChild(std::shared_ptr<Task> child)
		{
			children.push_back(child);

			child->parents.push_back(Shared());

		}

		void Run()
		{
			WaitParents();

			taskfunc();

			promise.set_value();

			finished = true;
		}
		bool Finished() const
		{
			return finished.load();
		}
		std::shared_ptr<Task> Shared()
		{
			return shared_from_this();
		}

	private:
		std::function<void()> taskfunc;

		std::vector<std::weak_ptr<Task>> parents;
		std::vector<std::shared_ptr<Task>> children;
		std::atomic_bool finished;

		std::promise<void> promise;
		std::shared_future<void> shared_future;

	private:
		void WaitParents()
		{
			for (auto& parent : parents) parent.lock()->shared_future.wait();
		}
	};
}