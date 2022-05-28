#include "RenderGraph.h"
#include "../Tasks/TaskSystem.h"

#include <algorithm>
#include <d3d12.h>
#include <wrl.h> 

namespace adria
{

	RGTextureHandle RenderGraphBuilder::CreateTexture(char const* name, TextureDesc const& desc)
	{
		RGTextureHandle handle = rg.CreateTexture(name, desc);
		rg_pass.creates.insert(handle);
		return handle;
	}

	RGTextureHandle RenderGraphBuilder::Read(RGTextureHandle handle, ERGReadFlag read_flag /*= ReadFlag_PixelShaderAccess*/)
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

	RGTextureHandle RenderGraphBuilder::Write(RGTextureHandle handle, ERGWriteFlag write_flag /*= WriteFlag_UnorderedAccess*/)
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

		auto const& node = rg.GetTextureNode(handle);
		if (node.texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		if (!rg_pass.creates.contains(handle)) ++node.texture->version;

		return handle;
	}

	RGTextureHandle RenderGraphBuilder::RenderTarget(RGTextureHandle handle, ERGLoadStoreAccessOp load_store_op)
	{
		rg_pass.resource_state_map[handle] = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rg_pass.writes.insert(handle);
		rg_pass.render_targets.push_back(RenderGraphPassBase::RenderTargetInfo{ .render_target_handle = handle, .render_target_access = load_store_op });
		auto const& node = rg.GetTextureNode(handle);
		if (node.texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		if (!rg_pass.creates.contains(handle)) ++node.texture->version;
		return handle;
	}

	RGTextureHandle RenderGraphBuilder::DepthStencil(RGTextureHandle handle, ERGLoadStoreAccessOp depth_load_store_op, bool readonly /*= false*/, ERGLoadStoreAccessOp stencil_load_store_op /*= ERGLoadStoreAccessOp::NoAccess_NoAccess*/)
	{
		rg_pass.reads.insert(handle);
		rg_pass.depth_stencil = RenderGraphPassBase::DepthStencilInfo{ .depth_stencil_handle = handle, .depth_access = depth_load_store_op,.stencil_access = stencil_load_store_op, .readonly = readonly };
		auto const& node = rg.GetTextureNode(handle);

		if (!rg_pass.creates.contains(handle)) ++node.texture->version;
		if (node.texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		rg_pass.resource_state_map[handle] = readonly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
		return handle;
	}

	RGTextureHandleSRV RenderGraphBuilder::CreateSRV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateSRV(handle, desc);
	}

	RGTextureHandleUAV RenderGraphBuilder::CreateUAV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateUAV(handle, desc);
	}

	RGTextureHandleRTV RenderGraphBuilder::CreateRTV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateRTV(handle, desc);
	}

	RGTextureHandleDSV RenderGraphBuilder::CreateDSV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		return rg.CreateDSV(handle, desc);
	}

	RenderGraphBuilder::RenderGraphBuilder(RenderGraph& rg, RenderGraphPassBase& rg_pass)
		: rg(rg), rg_pass(rg_pass)
	{}

	RGTextureHandle RenderGraph::CreateTexture(char const* name, TextureDesc const& desc)
	{
		textures.emplace_back(new RGTexture(name, textures.size(), desc));
		return CreateTextureNode(textures.back().get());
	}

	RGTextureHandle RenderGraph::ImportTexture(char const* name, Texture* texture)
	{
		imported_textures.emplace_back(new RGTexture(name, imported_textures.size(), texture));
		return CreateTextureNode(textures.back().get());
	}

	Texture* RenderGraph::GetTexture(RGTextureHandle handle) const
	{
		RGTextureNode const& node = GetTextureNode(handle);
		return node.texture->resource;
	}

	RGBufferHandle RenderGraph::CreateBuffer(char const* name, BufferDesc const& desc)
	{

	}

	Buffer* RenderGraph::GetBuffer(RGBufferHandle handle) const
	{
		RGBufferNode const& node = GetBufferNode(handle);
		return node.buffer->resource;
	}

	Texture* RenderGraph::GetImportedTexture(RGTextureHandle handle)
	{
		RGTextureNode const& node = GetTextureNode(handle);
		return node.texture->resource;
	}

	bool RenderGraph::IsValidTextureHandle(RGTextureHandle handle) const
	{
		return handle.IsValid() && handle.id < texture_nodes.size();
	}

	bool RenderGraph::IsValidBufferHandle(RGBufferHandle handle) const
	{
		return handle.IsValid() && handle.id < buffer_nodes.size();
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
				auto& rg_texture_node = GetTextureNode(handle);
				rg_texture_node.texture->resource = pool.AllocateTexture(rg_texture_node.texture->desc);
			}

			dependency_level.Execute(gfx, cmd_list);

			for (auto handle : dependency_level.destroys)
			{
				auto& rg_texture_node = GetTextureNode(handle);
				pool.ReleaseTexture(rg_texture_node.texture->resource);
			}
		}
	}

	RGTextureHandle RenderGraph::CreateTextureNode(RGTexture* resource)
	{
		texture_nodes.emplace_back(resource);
		return RGTextureHandle(texture_nodes.size() - 1);
	}

	RGTextureNode const& RenderGraph::GetTextureNode(RGTextureHandle handle) const
	{
		return texture_nodes[handle.id];
	}

	RGTextureNode& RenderGraph::GetTextureNode(RGTextureHandle handle)
	{
		return texture_nodes[handle.id];
	}

	RGBufferHandle RenderGraph::CreateBufferNode(RGBuffer* resource)
	{
		buffer_nodes.emplace_back(resource);
		return RGBufferHandle(buffer_nodes.size() - 1);
	}

	RGBufferNode const& RenderGraph::GetBufferNode(RGBufferHandle handle) const
	{
		return buffer_nodes[handle.id];
	}

	RGBufferNode& RenderGraph::GetBufferNode(RGBufferHandle handle)
	{
		return buffer_nodes[handle.id];
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
				auto& consumed = GetTextureNode(id);
				++consumed.texture->ref_count;
			}
			for (auto id : pass->writes)
			{
				auto& written = GetTextureNode(id);
				written.writer = pass.get();
			}
		}

		std::stack<RGTextureNode> zero_ref_resources;
		for (auto& node : texture_nodes) if (node.texture->ref_count == 0) zero_ref_resources.push(node);

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
					auto& node = GetTextureNode(id);
					if (--node.texture->ref_count == 0) zero_ref_resources.push(node);
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
				GetTextureNode(id).last_used_by = pass.get();
			for (auto id : pass->reads)
				GetTextureNode(id).last_used_by = pass.get();
		}

		for (size_t i = 0; i < texture_nodes.size(); ++i)
		{
			texture_nodes[i].last_used_by->destroy.insert(RGTextureHandle(i));
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

	RGTextureHandleSRV RenderGraph::CreateSRV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureHandleSRV(view_id, handle);
	}

	RGTextureHandleUAV RenderGraph::CreateUAV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureHandleUAV(view_id, handle);
	}

	RGTextureHandleRTV RenderGraph::CreateRTV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureHandleRTV(view_id, handle);
	}

	RGTextureHandleDSV RenderGraph::CreateDSV(RGTextureHandle handle, TextureViewDesc const& desc)
	{
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureHandleDSV(view_id, handle);
	}

	ResourceView RenderGraph::GetSRV(RGTextureHandleSRV handle) const
	{
		RGTextureHandle tex_handle = handle.GetTypedResourceHandle();
		std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
		TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

		Texture* texture = GetTexture(tex_handle);
		size_t i = texture->CreateSRV(&view_desc);
		return texture->SRV(i);
	}

	ResourceView RenderGraph::GetUAV(RGTextureHandleUAV handle) const
	{
		RGTextureHandle tex_handle = handle.GetTypedResourceHandle();
		std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
		TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

		Texture* texture = GetTexture(tex_handle);
		size_t i = texture->CreateUAV(&view_desc);
		return texture->UAV(i);
	}

	ResourceView RenderGraph::GetRTV(RGTextureHandleRTV handle) const
	{
		RGTextureHandle tex_handle = handle.GetTypedResourceHandle();
		std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
		TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

		Texture* texture = GetTexture(tex_handle);
		size_t i = texture->CreateRTV(&view_desc);
		return texture->RTV(i);
	}

	ResourceView RenderGraph::GetDSV(RGTextureHandleDSV handle) const
	{
		RGTextureHandle tex_handle = handle.GetTypedResourceHandle();
		std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
		TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

		Texture* texture = GetTexture(tex_handle);
		size_t i = texture->CreateDSV(&view_desc);
		return texture->DSV(i);
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

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, CommandList* cmd_list)
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

