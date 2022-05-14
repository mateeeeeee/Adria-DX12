#include "RenderGraph.h"
#include "../Tasks/TaskSystem.h"

#include <algorithm>
#include <d3d12.h>
#include <wrl.h> 

namespace adria
{

	RGResourceHandle RenderGraphBuilder::Create(char const* name, D3D12_RESOURCE_DESC const& desc)
	{
		RGResourceHandle handle = rg.CreateResource(name, desc);
		rg_pass.creates.insert(handle);
		return handle;
	}

	RGResourceHandle RenderGraphBuilder::Read(RGResourceHandle const& handle)
	{
		rg_pass.reads.insert(handle);
		return handle;
	}

	RGResourceHandle RenderGraphBuilder::Write(RGResourceHandle& handle)
	{
		if (rg_pass.creates.contains(handle))
		{
			rg_pass.writes.insert(handle);
			return handle;
		}
		else
		{
			auto const& node = rg.GetResourceNode(handle);
			++node.resource->version;
			if (node.resource->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
			handle = RGResourceHandle();
			RGResourceHandle new_handle = rg.CreateResourceNode(node.resource);
			rg_pass.writes.insert(new_handle);
			return new_handle;
		}
	}

	RenderGraphBuilder::RenderGraphBuilder(RenderGraph& rg, RenderGraphPassBase& rg_pass)
		: rg(rg), rg_pass(rg_pass)
	{}

	RGResourceHandle RenderGraph::CreateResource(char const* name, D3D12_RESOURCE_DESC const& desc)
	{
		resources.emplace_back(new RGResource(name, resources.size(), desc));
		return CreateResourceNode(resources.back().get());
	}

	RGResourceHandle RenderGraph::ImportResource(EImportedId id, ID3D12Resource* resource)
	{
		auto& imported_resource = resources.emplace_back(new RGResource(ImportedIdNames[(size_t)id], resources.size(), resource));
		imported_resources[(size_t)id] = imported_resource.get();
		return CreateResourceNode(resources.back().get());
	}

	void RenderGraph::ImportResourceView(EImportedViewId id, RGResourceView view)
	{
		imported_views[(size_t)id] = view;
	}

	bool RenderGraph::IsValidHandle(RGResourceHandle handle) const
	{
		return handle.IsValid() && handle.id < resource_nodes.size();
	}

	RGResource* RenderGraph::GetResource(RGResourceHandle handle) 
	{
		RGResourceNode& node = GetResourceNode(handle);
		return node.resource;
	}

	RGResource* RenderGraph::GetImportedResource(EImportedId id) const
	{
		return imported_resources[(size_t)id];
	}

	RGResourceView RenderGraph::GetImportedView(EImportedViewId id) const
	{
		return imported_views[(size_t)id];
	}

	void RenderGraph::Build()
	{
		BuildAdjacencyLists();
		TopologicalSort();
		BuildDependencyLevels();
		CullPasses();
		CalculateResourcesLifetime();
	}

	void RenderGraph::Execute()
	{
		pool.Tick();

		auto cmd_list = gfx->GetNewGraphicsCommandList();

		for (auto& dependency_level : dependency_levels)
		{
			for (auto handle : dependency_level.creates)
			{
				RGResource* rg_resource = GetResource(handle);
				rg_resource->resource = pool.AllocateResource(rg_resource->desc);
			}

			dependency_level.Execute(gfx, cmd_list);

			for (auto handle : dependency_level.destroys)
			{
				RGResource* rg_resource = GetResource(handle);
				pool.ReleaseResource(rg_resource->resource);
			}
		}
	}

	RGResourceHandle RenderGraph::CreateResourceNode(RGResource* resource)
	{
		resource_nodes.emplace_back(resource);
		return RGResourceHandle(resource_nodes.size() - 1);
	}

	RGResourceNode& RenderGraph::GetResourceNode(RGResourceHandle handle)
	{
		return resource_nodes[handle.id];
	}

	void RenderGraph::BuildAdjacencyLists()
	{
		adjacency_lists.resize(passes.size());
		for (auto i = 0; i < passes.size(); ++i)
		{
			auto& node = passes[i];
			std::vector<uint64_t>& adjacent_nodes_indices = adjacency_lists[i];
			for (auto j = 0; j < passes.size(); ++j)
			{
				if (i == j) continue;

				auto& other_node = passes[j];
				for (auto const& other_node_reads : other_node->reads)
				{
					bool depends = node->writes.find(other_node_reads) != node->writes.end();
					if (depends)
					{
						adjacent_nodes_indices.push_back(j);
						break;
					}
				}
			}
		}
	}

	void RenderGraph::TopologicalSort()
	{
		std::stack<size_t> stack{};
		std::vector<bool>  visited(passes.size(), false);
		for (size_t i = 0; i < passes.size(); i++)
		{
			if (visited[i] == false) DepthFirstSearch(i, visited, stack);
		}

		while (!stack.empty())
		{
			size_t i = stack.top();
			topologically_sorted_passes.push_back(passes[i].get());
			stack.pop();
		}
	}

	void RenderGraph::BuildDependencyLevels()
	{
		std::vector<size_t> distances(topologically_sorted_passes.size(), 0);
		for (size_t u = 0; u < topologically_sorted_passes.size(); ++u)
		{
			auto topologically_sorted_pass = topologically_sorted_passes[u];
			for (auto v : adjacency_lists[u])
			{
				if (distances[v] < distances[u] + 1) distances[v] = distances[u] + 1;
			}
		}

		dependency_levels.resize(*std::max_element(std::begin(distances), std::end(distances)) + 1, DependencyLevel(*this));
		for (size_t i = 0; i < topologically_sorted_passes.size(); ++i)
		{
			size_t level = distances[i];
			dependency_levels[level].AddPass(topologically_sorted_passes[i]);
		}
	}

	void RenderGraph::CullPasses()
	{
		for (auto& pass : passes)
		{
			pass->ref_count = pass->writes.size();
			for (auto id : pass->reads)
			{
				auto& consumed = GetResourceNode(id);
				++consumed.resource->ref_count;
			}
			for (auto id : pass->writes)
			{
				auto& written = GetResourceNode(id);
				written.writer = pass.get();
			}
		}

		std::stack<RGResourceNode> zero_ref_resources;
		for (auto& node : resource_nodes) if (node.resource->ref_count == 0) zero_ref_resources.push(node);

		while (!zero_ref_resources.empty())
		{
			RGResourceNode unreferenced_resource = zero_ref_resources.top();
			zero_ref_resources.pop();
			auto* writer = unreferenced_resource.writer;
			if (writer == nullptr || !writer->CanBeCulled()) continue;

			if (--writer->ref_count == 0)
			{
				for (auto id : writer->reads)
				{
					auto& node = GetResourceNode(id);
					if (--node.resource->ref_count == 0) zero_ref_resources.push(node);
				}
			}
		}
	}

	void RenderGraph::CalculateResourcesLifetime()
	{
		for (auto& pass : passes) 
		{
			if (pass->IsCulled()) continue;
			for (auto id : pass->writes)
				GetResourceNode(id).last_used_by = pass.get();
			for (auto id : pass->reads)
				GetResourceNode(id).last_used_by = pass.get();
		}

		for (size_t i = 0; i < resource_nodes.size(); ++i)
		{
			resource_nodes[i].last_used_by->destroy.insert(RGResourceHandle(i));
		}

		for (auto& dependency_level : dependency_levels)
		{
			dependency_level.SetupDestroys();
		}
	}

	void RenderGraph::DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack)
	{
		visited[i] = true;
		for (auto j : adjacency_lists[i])
		{
			if (!visited[j]) DepthFirstSearch(j, visited, stack);
		}
		stack.push(i);
	}

	RGResourceView RenderGraph::CreateShaderResourceView(RGResourceHandle handle, D3D12_SHADER_RESOURCE_VIEW_DESC const& desc)
	{
		return view_pool.CreateShaderResourceView(handle, desc);
	}

	RGResourceView RenderGraph::CreateRenderTargetView(RGResourceHandle handle, D3D12_RENDER_TARGET_VIEW_DESC const& desc)
	{
		return view_pool.CreateRenderTargetView(handle, desc);
	}

	RGResourceView RenderGraph::CreateUnorderedAccessView(RGResourceHandle handle, D3D12_UNORDERED_ACCESS_VIEW_DESC const& desc)
	{
		return view_pool.CreateUnorderedAccessView(handle, desc);
	}

	RGResourceView RenderGraph::CreateDepthStencilView(RGResourceHandle handle, D3D12_DEPTH_STENCIL_VIEW_DESC const& desc)
	{
		return view_pool.CreateDepthStencilView(handle, desc);
	}

	void RenderGraph::DependencyLevel::AddPass(RenderGraphPassBase* pass)
	{
		passes.push_back(pass);
		reads.insert(pass->reads.begin(), pass->reads.end());
		writes.insert(pass->writes.begin(), pass->writes.end());
		creates.insert(pass->creates.begin(), pass->creates.end());
	}

	void RenderGraph::DependencyLevel::SetupDestroys()
	{
		for (auto& pass : passes) destroys.insert(pass->destroy.begin(), pass->destroy.end());
	}

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, ID3D12GraphicsCommandList4* cmd_list)
	{
		for (auto& pass : passes)
		{
			RenderGraphResources rg_resources(rg, *pass);
			if(!pass->IsCulled()) pass->Execute(rg_resources, gfx, cmd_list);
		}
	}

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, std::vector<ID3D12GraphicsCommandList4*> const& cmd_lists)
	{
		ADRIA_ASSERT(passes.size() <= cmd_lists.size());
		std::vector<std::future<void>> void_futures;

		for (size_t i = 0; i < passes.size(); ++i)
		{
			if (passes[i]->IsCulled()) continue;
			void_futures.push_back(
				TaskSystem::Submit([&](size_t j) 
					{
						RenderGraphResources rg_resources(rg, *passes[j]);
						passes[j]->Execute(rg_resources, gfx, cmd_lists[j]);
					}, i));
		}
		for (auto& fut : void_futures) fut.wait();
	}

	size_t RenderGraph::DependencyLevel::GetSize() const
	{
		return passes.size();
	}

	size_t RenderGraph::DependencyLevel::GetNonCulledSize() const
	{
		return std::count_if(std::begin(passes), std::end(passes), [](auto* pass) {return !pass->IsCulled(); });
	}

}

