#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_set>
#include <d3d12.h>
#include "../Core/Macros.h"

namespace adria
{

	using ResourceId = size_t;

	struct FrameGraphResource
	{
		FrameGraphResource(ID3D12Resource* resource)
			: resource(resource), resource_desc(resource->GetDesc())
		{
		}

	private:
		ID3D12Resource* resource;
		D3D12_RESOURCE_DESC resource_desc;

		friend class FrameGraph;
	};

	struct FrameGraphTaskBase
	{
		std::unordered_set<FrameGraphResource> read_resources;
		std::unordered_set<FrameGraphResource> write_resources;
		std::string name;
	private:
		mutable uint64_t unordered_index = -1;
		mutable uint64_t dependency_level_index = -1;

	private:
		virtual void Setup() = 0;
		virtual void Execute() const = 0;

		friend class FrameGraph;
	};

	template<typename _D>
	class FrameGraphTask : public FrameGraphTaskBase
	{
		using data_t = _D;
		using setup_callback_t = std::function<void(data_t&)>;
		using execute_callback_t = std::function<void(data_t const&)>;

	public:
		FrameGraphTask(data_t const& data, setup_callback_t&& setup,
			execute_callback_t&& execute)
			: task_data(data), setup(std::forward<setup_callback_t>(setup)),
			execute(std::forward<execute_callback_t>(execute))
		{
		}

		FrameGraphTask(data_t const& data,
			execute_callback_t&& execute)
			: task_data(data), setup(nullptr),
			execute(std::forward<execute_callback_t>(execute))
		{
		}

	private:
		data_t task_data;
		setup_callback_t setup;
		execute_callback_t execute;

	private:
		virtual void Setup() override
		{
			if(setup) setup(task_data);
		}
		virtual void Execute() const override
		{
			if (execute) execute(task_data);
		}
	};



	class FrameGraph
	{
	public:
		FrameGraph() = default;

		void AddTask(FrameGraphTaskBase* task)
		{
			task->unordered_index = unordered_tasks.size();
			task->Setup();
			unordered_tasks.emplace_back(task);
		}

		template<typename _TaskData, typename... _Args>
		void AddTask(_TaskData const& data, _Args&&... args) //add concept check for args and
		{
			FrameGraphTaskBase const* task = new FrameGraphTask<_TaskData>(data, std::forward<Args>(args)...);
			task->unordered_index = unordered_tasks.size();
			task->Setup();
			unordered_tasks.emplace_back(task);
		}

		void Build()
		{
			BuildAdjacencyLists();
			TopologicalSort();
			FindDependencyLevels();
		}

		void Execute()
		{
			for (auto const& dependency_level : dependency_levels)
			{
				for (uint64_t task_index : dependency_level)
				{
					unordered_tasks[task_index]->Execute();
				}
			}
			Clear();
		}

	private:
		std::vector<std::unique_ptr<FrameGraphTaskBase const>> unordered_tasks;
		std::vector<std::list<uint64_t>> adjacency_lists;

		std::vector<FrameGraphTaskBase const*> topologically_ordered_tasks;
		std::vector<std::vector<uint64_t>> dependency_levels;

	private:

		void Clear()
		{
			unordered_tasks.clear();
			adjacency_lists.clear();
			topologically_ordered_tasks.clear();
			dependency_levels.clear();
		}

		void BuildAdjacencyLists()
		{
			adjacency_lists.resize(unordered_tasks.size());

			for (size_t i = 0; i < unordered_tasks.size(); ++i)
			{
				FrameGraphTaskBase const* task = unordered_tasks[i].get();
				if (task->read_resources.empty() && task->write_resources.empty()) continue;

				std::list<uint64_t>& adjacency_list = adjacency_lists[i];

				for (size_t j = 0; j < unordered_tasks.size(); ++j)
				{
					if (i == j) continue;

					FrameGraphTaskBase const* other_task = unordered_tasks[j].get();

					auto CheckDependency = [&task, &adjacency_list, j](FrameGraphResource const& other_read_handle)
					{
						bool has_dependency = task->write_resources.find(other_read_handle) != task->write_resources.end();

						if (has_dependency) adjacency_list.push_back(j);

						return has_dependency;
					};

					for (auto& read_handle : other_task->read_resources)
					{
						if (CheckDependency(read_handle)) break;
					}
				}
			}
		}

		void DFS(uint64_t node_index, std::vector<bool>& visited, std::vector<bool>& on_stack, bool& is_cyclic)
		{
			if (is_cyclic)
				return;

			visited[node_index] = true;
			on_stack[node_index] = true;

			for (uint64_t neighbour : adjacency_lists[node_index])
			{
				if (visited[neighbour] && on_stack[neighbour])
				{
					is_cyclic = true;
					return;
				}

				if (!visited[neighbour])
				{
					DFS(neighbour, visited, on_stack, is_cyclic);
				}
			}

			on_stack[node_index] = false;
			topologically_ordered_tasks.push_back(unordered_tasks[node_index].get());
		}

		void TopologicalSort()
		{
			std::vector<bool> visited_nodes(unordered_tasks.size(), false);
			std::vector<bool> on_stack_nodes(unordered_tasks.size(), false);

			bool is_cyclic = false;

			for (size_t i = 0; i < unordered_tasks.size(); ++i)
			{
				FrameGraphTaskBase const* task = unordered_tasks[i].get();

				if (!visited_nodes[i] && !(task->read_resources.empty() && task->write_resources.empty()))
				{
					DFS(i, visited_nodes, on_stack_nodes, is_cyclic);
					ADRIA_ASSERT(!is_cyclic);
				}
			}

			std::reverse(topologically_ordered_tasks.begin(), topologically_ordered_tasks.end());
		}

		void FindDependencyLevels()
		{
			std::vector<int64_t> longest_distances(unordered_tasks.size(), 0);
			uint64_t dependency_level_count = 1;

			// Perform longest node distance search
			for (size_t i = 0; i < topologically_ordered_tasks.size(); ++i)
			{
				uint64_t original_index = topologically_ordered_tasks[i]->unordered_index;

				for (uint64_t adjacent_node_index : adjacency_lists[original_index])
				{
					if (longest_distances[adjacent_node_index] < longest_distances[original_index] + 1)
					{
						int64_t new_longest_distance = longest_distances[original_index] + 1;
						longest_distances[adjacent_node_index] = new_longest_distance;
						dependency_level_count = std::max<uint64_t>(uint64_t(new_longest_distance + 1), dependency_level_count);
					}
				}
			}

			dependency_levels.resize(dependency_level_count);

			for (size_t i = 0; i < topologically_ordered_tasks.size(); ++i)
			{
				FrameGraphTaskBase const* task = topologically_ordered_tasks[i];
				uint64_t level_index = longest_distances[task->unordered_index];
				std::vector<uint64_t> & dependency_level = dependency_levels[level_index];
				dependency_level.push_back(task->unordered_index);
				task->dependency_level_index = level_index;
			}
		}

	};

}
 