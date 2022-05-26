#include "RenderGraph.h"
#include "../Tasks/TaskSystem.h"

#include <algorithm>
#include <d3d12.h>
#include <wrl.h> 

namespace adria
{

	RGResourceHandle RenderGraphBuilder::CreateTexture(char const* name, TextureDesc const& desc)
	{
		RGResourceHandle handle = rg.CreateTexture(name, desc);
		rg_pass.creates.insert(handle);
		return handle;
	}

	RGResourceHandle RenderGraphBuilder::Read(RGResourceHandle const& handle, ERGReadFlag read_flag)
	{
		if (rg_pass.type == ERGPassType::Copy) read_flag = ReadFlag_CopySrc;

		rg_pass.reads.insert(handle);
		switch (read_flag)
		{
		case ReadFlag_PixelShaderAccess:
			rg_pass.resource_state_map[handle] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			break;
		case ReadFlag_NonPixelShaderAccess:
			rg_pass.resource_state_map[handle] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			break;
		case ReadFlag_AllPixelShaderAccess:
			rg_pass.resource_state_map[handle] = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
			break;
		case ReadFlag_CopySrc:
			rg_pass.resource_state_map[handle] = D3D12_RESOURCE_STATE_COPY_SOURCE;
			break;
		case ReadFlag_IndirectArgument:
			rg_pass.resource_state_map[handle] = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Read Flag!");
		}

		return handle;
	}

	RGResourceHandle RenderGraphBuilder::Write(RGResourceHandle& handle, ERGWriteFlag write_flag)
	{
		if (rg_pass.type == ERGPassType::Copy) write_flag = WriteFlag_CopyDst;

		D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
		switch (write_flag)
		{
		case WriteFlag_UnorderedAccess:
			resource_states = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			break;
		case WriteFlag_CopyDst:
			resource_states = D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Read Flag!");
		}

		rg_pass.resource_state_map[handle] = resource_states;
		rg_pass.writes.insert(handle);

		auto const& node = rg.GetResourceNode(handle);
		if (node.resource->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		if (!rg_pass.creates.contains(handle)) ++node.resource->version;

		return handle;
	}

	RGResourceHandle RenderGraphBuilder::RenderTarget(RGResourceHandle& handle, ERGLoadStoreAccessOp load_store_op)
	{
		rg_pass.resource_state_map[handle] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rg_pass.writes.insert(handle);
		rg_pass.render_targets.push_back(RenderGraphPassBase::RenderTargetInfo{ .render_target_handle = handle, .render_target_access = load_store_op });
		auto const& node = rg.GetResourceNode(handle);
		if (node.resource->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		if (!rg_pass.creates.contains(handle)) ++node.resource->version;
		return handle;
	}

	RGResourceHandle RenderGraphBuilder::DepthStencil(RGResourceHandle& handle, ERGLoadStoreAccessOp depth_load_store_op, bool readonly /*= false*/, ERGLoadStoreAccessOp stencil_load_store_op /*= ERGLoadStoreAccessOp::NoAccess_NoAccess*/)
	{
		rg_pass.reads.insert(handle);
		rg_pass.depth_stencil = RenderGraphPassBase::DepthStencilInfo{ .depth_stencil_handle = handle, .depth_access = depth_load_store_op,.stencil_access = stencil_load_store_op, .readonly = readonly };
		auto const& node = rg.GetResourceNode(handle);

		if (!rg_pass.creates.contains(handle)) ++node.resource->version;
		if (node.resource->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		rg_pass.resource_state_map[handle] = readonly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
		return handle;
	}

	RenderGraphBuilder::RenderGraphBuilder(RenderGraph& rg, RenderGraphPassBase& rg_pass)
		: rg(rg), rg_pass(rg_pass)
	{}

	RGResourceHandle RenderGraph::CreateTexture(char const* name, TextureDesc const& desc)
	{
		textures.emplace_back(new RGTexture(name, textures.size(), desc));
		return CreateResourceNode(textures.back().get());
	}

	RGResourceHandle RenderGraph::ImportResource(EImportedId id, ID3D12Resource* texture)
	{
		imported_resources[(size_t)id] = texture;
	}

	void RenderGraph::ImportResourceView(EImportedViewId id, RGResourceView view)
	{
		imported_views[(size_t)id] = view;
	}

	bool RenderGraph::IsValidHandle(RGResourceHandle handle) const
	{
		return handle.IsValid() && handle.id < texture_nodes.size();
	}

	RGTexture* RenderGraph::GetTexture(RGResourceHandle handle) 
	{
		RGTextureNode& node = GetResourceNode(handle);
		return node.resource;
	}

	ID3D12Resource* RenderGraph::GetImportedResource(EImportedId id) const
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
				RGTexture* rg_texture = GetTexture(handle);
				rg_texture->texture = pool.AllocateTexture(rg_texture->desc);
			}

			dependency_level.Execute(gfx, cmd_list);

			for (auto handle : dependency_level.destroys)
			{
				RGTexture* rg_texture = GetTexture(handle);
				pool.ReleaseTexture(rg_texture->texture);
			}
		}
	}

	RGResourceHandle RenderGraph::CreateResourceNode(RGTexture* resource)
	{
		texture_nodes.emplace_back(resource);
		return RGResourceHandle(texture_nodes.size() - 1);
	}

	RGTextureNode& RenderGraph::GetResourceNode(RGResourceHandle handle)
	{
		return texture_nodes[handle.id];
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

		std::stack<RGTextureNode> zero_ref_resources;
		for (auto& node : texture_nodes) if (node.resource->ref_count == 0) zero_ref_resources.push(node);

		while (!zero_ref_resources.empty())
		{
			RGTextureNode unreferenced_resource = zero_ref_resources.top();
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

		for (size_t i = 0; i < texture_nodes.size(); ++i)
		{
			texture_nodes[i].last_used_by->destroy.insert(RGResourceHandle(i));
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

	RGResourceView RenderGraph::CreateShaderResourceView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		RGTexture* texture = GetTexture(handle);
		size_t i = texture->texture->CreateSRV(&desc);
		return texture->texture->SRV(i);
	}

	RGResourceView RenderGraph::CreateRenderTargetView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		RGTexture* texture = GetTexture(handle);
		size_t i = texture->texture->CreateRTV(&desc);
		return texture->texture->SRV(i);
	}

	RGResourceView RenderGraph::CreateUnorderedAccessView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		RGTexture* texture = GetTexture(handle);
		size_t i = texture->texture->CreateUAV(&desc);
		return texture->texture->SRV(i);
	}

	RGResourceView RenderGraph::CreateDepthStencilView(RGResourceHandle handle, TextureViewDesc const& desc)
	{
		RGTexture* texture = GetTexture(handle);
		size_t i = texture->texture->CreateDSV(&desc);
		return texture->texture->SRV(i);
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
			if (pass->IsCulled()) continue;
			RenderGraphResources rg_resources(rg, *pass);
			pass->Execute(rg_resources, gfx, cmd_list);
		}
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

