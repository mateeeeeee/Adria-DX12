#include "RenderGraph.h"
#include "../Graphics/RenderPass.h"
#include "../Graphics/ResourceBarrierBatch.h"
#include "../Tasks/TaskSystem.h"
#include "../Utilities/StringUtil.h"

#include <algorithm>
#include <d3d12.h>
#include <wrl.h> 
#include "pix3.h"


namespace adria
{
	RGTextureId RenderGraph::DeclareTexture(RGResourceName name, TextureDesc const& desc)
	{
		textures.emplace_back(new RGTexture(textures.size(), desc));
		texture_name_id_map[name] = RGTextureId(textures.size() - 1);
		return RGTextureId(textures.size() - 1);
	}

	RGBufferId RenderGraph::DeclareBuffer(RGResourceName name, BufferDesc const& desc)
	{
		buffers.emplace_back(new RGBuffer(buffers.size(), desc));
		buffer_name_id_map[name] = RGBufferId(buffers.size() - 1);
		return RGBufferId(buffers.size() - 1);
	}

	RGAllocationId RenderGraph::DeclareAllocation(RGResourceName name, AllocDesc const& alloc)
	{
		dynamic_allocations.emplace_back(gfx->GetDynamicAllocator()->Allocate(alloc.size_in_bytes, alloc.alignment));
		alloc_name_id_map[name] = RGAllocationId{ dynamic_allocations.size() - 1 };
		return RGAllocationId{ dynamic_allocations.size() - 1 };
	}

	bool RenderGraph::IsTextureDeclared(RGResourceName name)
	{
		return texture_name_id_map.find(name) != texture_name_id_map.end();
	}

	bool RenderGraph::IsBufferDeclared(RGResourceName name)
	{
		return buffer_name_id_map.find(name) != buffer_name_id_map.end();
	}

	bool RenderGraph::IsAllocationDeclared(RGResourceName name)
	{
		return alloc_name_id_map.find(name) != alloc_name_id_map.end();
	}

	void RenderGraph::ImportTexture(RGResourceName name, Texture* texture)
	{
		textures.emplace_back(new RGTexture(textures.size(), texture));
		texture_name_id_map[name] = RGTextureId(textures.size() - 1);
	}

	void RenderGraph::ImportBuffer(RGResourceName name, Buffer* buffer)
	{
		buffers.emplace_back(new RGBuffer(buffers.size(), buffer));
		buffer_name_id_map[name] = RGBufferId(buffers.size() - 1);
	}

	bool RenderGraph::IsValidTextureHandle(RGTextureId handle) const
	{
		return handle.IsValid() && handle.id < textures.size();
	}

	bool RenderGraph::IsValidBufferHandle(RGBufferId handle) const
	{
		return handle.IsValid() && handle.id < buffers.size();
	}

	bool RenderGraph::IsValidAllocHandle(RGAllocationId alloc) const
	{
		return alloc.id < dynamic_allocations.size();
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
#ifdef RG_MULTITHREADED
		Execute_Multithreaded();
#else
		Execute_Singlethreaded();
#endif
	}

	void RenderGraph::Execute_Singlethreaded()
	{
		pool.Tick();
		for (auto& dependency_level : dependency_levels) dependency_level.Setup();

		auto cmd_list = gfx->GetDefaultCommandList();
		for (size_t i = 0; i < dependency_levels.size(); ++i)
		{
			auto& dependency_level = dependency_levels[i];
			for (auto tex_id : dependency_level.texture_creates)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				if(!rg_texture->imported) rg_texture->resource = pool.AllocateTexture(rg_texture->desc);
			}
			for (auto buf_id : dependency_level.buffer_creates)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				if (!rg_buffer->imported) rg_buffer->resource = pool.AllocateBuffer(rg_buffer->desc);
			}

			ResourceBarrierBatch barrier_batcher{};
			{
				for (auto const& [tex_id, state] : dependency_level.texture_state_map)
				{
					RGTexture* rg_texture = GetRGTexture(tex_id);
					Texture* texture = rg_texture->resource;
					if (dependency_level.texture_creates.contains(tex_id))
					{
						if (!HasAllFlags(texture->GetDesc().initial_state, state))
						{
							barrier_batcher.AddTransition(texture->GetNative(), texture->GetDesc().initial_state, state);
						}
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.texture_state_map.contains(tex_id))
						{
							RGResourceState prev_state = prev_dependency_level.texture_state_map[tex_id];
							if (prev_state != state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, state); //!HasAllFlags(prev_state, state)
							found = true;
							break;
						}
					}
					if (!found && rg_texture->imported)
					{
						RGResourceState prev_state = rg_texture->desc.initial_state;
						if (prev_state != state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, state); //!HasAllFlags(prev_state, state)
					}
				}
				for (auto const& [buf_id, state] : dependency_level.buffer_state_map)
				{
					RGBuffer* rg_buffer = GetRGBuffer(buf_id);
					Buffer* buffer = rg_buffer->resource;
					if (dependency_level.buffer_creates.contains(buf_id))
					{
						if (state != D3D12_RESOURCE_STATE_COMMON) //check if there is an implicit transition, maybe this can be avoided
						{
							barrier_batcher.AddTransition(buffer->GetNative(), D3D12_RESOURCE_STATE_COMMON, state);
						}
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.buffer_state_map.contains(buf_id))
						{
							RGResourceState prev_state = prev_dependency_level.buffer_state_map[buf_id];
							if (prev_state != state) barrier_batcher.AddTransition(buffer->GetNative(), prev_state, state); 
							found = true;
							break;
						}
					}
					if (!found && rg_buffer->imported)
					{
						RGResourceState prev_state = D3D12_RESOURCE_STATE_COMMON;
						if (prev_state != state) barrier_batcher.AddTransition(buffer->GetNative(), prev_state, state);
					}
				}
			}

			barrier_batcher.Submit(cmd_list);
			dependency_level.Execute(gfx, cmd_list);

			barrier_batcher.Clear();
			for (RGTextureId tex_id : dependency_level.texture_destroys)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				Texture* texture = rg_texture->resource;
				RGResourceState initial_state = texture->GetDesc().initial_state;
				ADRIA_ASSERT(dependency_level.texture_state_map.contains(tex_id));
				RGResourceState state = dependency_level.texture_state_map[tex_id];
				if (initial_state != state) barrier_batcher.AddTransition(texture->GetNative(), state, initial_state);
				if (!rg_texture->imported) pool.ReleaseTexture(rg_texture->resource);
			}
			for (RGBufferId buf_id : dependency_level.buffer_destroys)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				Buffer* buffer = rg_buffer->resource;
				RGResourceState initial_state = D3D12_RESOURCE_STATE_COMMON;
				ADRIA_ASSERT(dependency_level.buffer_state_map.contains(buf_id));
				RGResourceState state = dependency_level.buffer_state_map[buf_id];
				if (initial_state != state) barrier_batcher.AddTransition(buffer->GetNative(), state, initial_state);
				if (!rg_buffer->imported) pool.ReleaseBuffer(rg_buffer->resource);
			}
			barrier_batcher.Submit(cmd_list);
		}
	}

	void RenderGraph::Execute_Multithreaded()
	{
#ifdef RG_MULTITHREADED
		static_assert(false, "Todo");
#endif
	}

	void RenderGraph::BuildAdjacencyLists()
	{
		adjacency_lists.resize(passes.size());
		for (size_t i = 0; i < passes.size(); ++i)
		{
			auto& pass = passes[i];
			std::vector<uint64>& pass_adjacency_list = adjacency_lists[i];
			for (size_t j = i + 1; j < passes.size(); ++j)
			{
				auto& other_pass = passes[j];
				bool depends = false;
				for (auto other_node_read : other_pass->texture_reads)
				{
					if (pass->texture_writes.find(other_node_read) != pass->texture_writes.end())
					{
						pass_adjacency_list.push_back(j);
						depends = true;
						break;
					}
				}
				if (depends) continue;

				for (auto other_node_read : other_pass->buffer_reads)
				{
					if (pass->buffer_writes.find(other_node_read) != pass->buffer_writes.end())
					{
						pass_adjacency_list.push_back(j);
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
			topologically_sorted_passes.push_back(i);
			stack.pop();
		}
	}

	void RenderGraph::BuildDependencyLevels()
	{
		std::vector<size_t> distances(topologically_sorted_passes.size(), 0);
		for (size_t u = 0; u < topologically_sorted_passes.size(); ++u)
		{
			size_t i = topologically_sorted_passes[u];
			for (auto v : adjacency_lists[i])
			{
				if (distances[v] < distances[i] + 1) distances[v] = distances[i] + 1;
			}
		}

		dependency_levels.resize(*std::max_element(std::begin(distances), std::end(distances)) + 1, DependencyLevel(*this));
		for (size_t i = 0; i < passes.size(); ++i)
		{
			size_t level = distances[i];
			dependency_levels[level].AddPass(passes[i].get());
		}
	}

	void RenderGraph::CullPasses()
	{
		for (auto& pass : passes)
		{
			pass->ref_count = pass->texture_writes.size() + pass->buffer_writes.size();
			for (auto id : pass->texture_reads)
			{
				auto* consumed = GetRGTexture(id);
				++consumed->ref_count;
			}
			for (auto id : pass->buffer_reads)
			{
				auto* consumed = GetRGBuffer(id);
				++consumed->ref_count;
			}

			for (auto id : pass->texture_writes)
			{
				auto* written = GetRGTexture(id);
				written->writer = pass.get();
			}
			for (auto id : pass->buffer_writes)
			{
				auto* written = GetRGBuffer(id);
				written->writer = pass.get();
			}
		}

		std::stack<RenderGraphResource*> zero_ref_resources;
		for (auto& texture : textures) if (texture->ref_count == 0) zero_ref_resources.push(texture.get());
		for (auto& buffer : buffers)   if (buffer->ref_count == 0) zero_ref_resources.push(buffer.get());

		while (!zero_ref_resources.empty())
		{
			RenderGraphResource* unreferenced_resource = zero_ref_resources.top();
			zero_ref_resources.pop();
			auto* writer = unreferenced_resource->writer;
			if (writer == nullptr || !writer->CanBeCulled()) continue;

			if (--writer->ref_count == 0)
			{
				for (auto id : writer->texture_reads)
				{
					auto* texture = GetRGTexture(id);
					if (--texture->ref_count == 0) zero_ref_resources.push(texture);
				}
				for (auto id : writer->buffer_reads)
				{
					auto* buffer = GetRGBuffer(id);
					if (--buffer->ref_count == 0) zero_ref_resources.push(buffer);
				}
			}
		}
	}

	void RenderGraph::CalculateResourcesLifetime()
	{
		for (size_t i = 0; i < topologically_sorted_passes.size(); ++i)
		{
			auto& pass = passes[topologically_sorted_passes[i]];
			if (pass->IsCulled()) continue;
			for (auto id : pass->texture_writes)
			{
				RGTexture* rg_texture = GetRGTexture(id);
				rg_texture->last_used_by = pass.get();
			}
			for (auto id : pass->buffer_writes)
			{
				RGBuffer* rg_buffer = GetRGBuffer(id);
				rg_buffer->last_used_by = pass.get();
			}
				
			for (auto id : pass->texture_reads)
			{
				RGTexture* rg_texture = GetRGTexture(id);
				rg_texture->last_used_by = pass.get();
			}
			for (auto id : pass->buffer_reads)
			{
				RGBuffer* rg_buffer = GetRGBuffer(id);
				rg_buffer->last_used_by = pass.get();
			}
		}
		for (size_t i = 0; i < textures.size(); ++i)
		{
			if(textures[i]->last_used_by != nullptr) textures[i]->last_used_by->texture_destroys.insert(RGTextureId(i));
		}
		for (size_t i = 0; i < buffers.size(); ++i)
		{
			if (buffers[i]->last_used_by != nullptr) buffers[i]->last_used_by->buffer_destroys.insert(RGBufferId(i));
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

	RGTexture* RenderGraph::GetRGTexture(RGTextureId handle) const
	{
		return textures[handle.id].get();
	}

	RGBuffer* RenderGraph::GetRGBuffer(RGBufferId handle) const
	{
		return buffers[handle.id].get();
	}

	Texture* RenderGraph::GetTexture(RGTextureId res_id) const
	{
		return GetRGTexture(res_id)->resource;
	}

	Buffer* RenderGraph::GetBuffer(RGBufferId res_id) const
	{
		return GetRGBuffer(res_id)->resource;
	}

	RGTextureId RenderGraph::GetTextureId(RGResourceName name)
	{
		ADRIA_ASSERT(IsTextureDeclared(name));
		return texture_name_id_map[name];
	}

	RGBufferId RenderGraph::GetBufferId(RGResourceName name)
	{
		ADRIA_ASSERT(IsBufferDeclared(name));
		return buffer_name_id_map[name];
	}

	RGTextureCopySrcId RenderGraph::ReadCopySrcTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		return RGTextureCopySrcId(handle);
	}

	RGTextureCopyDstId RenderGraph::WriteCopyDstTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		return RGTextureCopyDstId(handle);
	}

	RGBufferCopySrcId RenderGraph::ReadCopySrcBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferCopySrcId(handle);
	}

	RGBufferCopyDstId RenderGraph::WriteCopyDstBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferCopyDstId(handle);
	}

	RGBufferIndirectArgsId RenderGraph::ReadIndirectArgsBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		return RGBufferIndirectArgsId(handle);
	}

	RGRenderTargetId RenderGraph::RenderTarget(RGResourceName name, TextureViewDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGRenderTargetId(view_id, handle);
	}

	RGDepthStencilId RenderGraph::DepthStencil(RGResourceName name, TextureViewDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGDepthStencilId(view_id, handle);
	}

	RGTextureReadOnlyId RenderGraph::ReadTexture(RGResourceName name, TextureViewDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureReadOnlyId(view_id, handle);
	}

	RGTextureReadWriteId RenderGraph::WriteTexture(RGResourceName name, TextureViewDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureReadWriteId(view_id, handle);
	}

	RGBufferReadOnlyId RenderGraph::ReadBuffer(RGResourceName name, BufferViewDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		std::vector<BufferViewDesc>& view_descs = buffer_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGBufferReadOnlyId(view_id, handle);
	}

	RGBufferReadWriteId RenderGraph::WriteBuffer(RGResourceName name, BufferViewDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		std::vector<BufferViewDesc>& view_descs = buffer_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGBufferReadWriteId(view_id, handle);
	}

	RGAllocationId RenderGraph::UseAllocation(RGResourceName name)
	{
		RGAllocationId alloc = alloc_name_id_map[name];
		ADRIA_ASSERT(IsValidAllocHandle(alloc) && "Allocation has not been declared!");
		return alloc;
	}

	DynamicAllocation& RenderGraph::GetAllocation(RGAllocationId alloc)
	{
		return dynamic_allocations[alloc.id];
	}

	Texture const& RenderGraph::GetResource(RGTextureCopySrcId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	Texture const& RenderGraph::GetResource(RGTextureCopyDstId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	Buffer const& RenderGraph::GetResource(RGBufferCopySrcId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	Buffer const& RenderGraph::GetResource(RGBufferCopyDstId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	Buffer const& RenderGraph::GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	RGDescriptor RenderGraph::GetDescriptor(RGRenderTargetId res_id) const
	{
#ifdef RG_MULTITHREADED
		std::scoped_lock lock(rtv_cache_mutex);
#endif
		if (auto it = texture_rtv_cache.find(res_id); it != texture_rtv_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureId tex_id = res_id.GetResourceId();
			std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[tex_id];
			TextureViewDesc const& view_desc = view_descs[res_id.GetViewId()];

			Texture* texture = GetTexture(tex_id);
			RGDescriptor rtv = texture->CreateAndTakeRTV(&view_desc);
			texture_rtv_cache.insert(std::make_pair(res_id, rtv));
			return rtv;
		}
	}

	RGDescriptor RenderGraph::GetDescriptor(RGDepthStencilId res_id) const
	{
#ifdef RG_MULTITHREADED
		std::scoped_lock lock(dsv_cache_mutex);
#endif
		if (auto it = texture_dsv_cache.find(res_id); it != texture_dsv_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureId tex_id = res_id.GetResourceId();
			std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[tex_id];
			TextureViewDesc const& view_desc = view_descs[res_id.GetViewId()];

			Texture* texture = GetTexture(tex_id);
			RGDescriptor dsv = texture->CreateAndTakeDSV(&view_desc);
			texture_dsv_cache.insert(std::make_pair(res_id, dsv));
			return dsv;
		}
	}

	RGDescriptor RenderGraph::GetDescriptor(RGTextureReadOnlyId res_id) const
	{
#ifdef RG_MULTITHREADED
		std::scoped_lock lock(srv_cache_mutex);
#endif
		if (auto it = texture_srv_cache.find(res_id); it != texture_srv_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureId tex_id = res_id.GetResourceId();
			std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[tex_id];
			TextureViewDesc const& view_desc = view_descs[res_id.GetViewId()];

			Texture* texture = GetTexture(tex_id);
			RGDescriptor srv = texture->CreateAndTakeSRV(&view_desc);
			texture_srv_cache.insert(std::make_pair(res_id, srv));
			return srv;
		}
	}

	RGDescriptor RenderGraph::GetDescriptor(RGTextureReadWriteId res_id) const
	{
#ifdef RG_MULTITHREADED
		std::scoped_lock lock(uav_cache_mutex);
#endif
		if (auto it = texture_uav_cache.find(res_id); it != texture_uav_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureId tex_id = res_id.GetResourceId();
			std::vector<TextureViewDesc>& view_descs = texture_view_desc_map[tex_id];
			TextureViewDesc const& view_desc = view_descs[res_id.GetViewId()];

			Texture* texture = GetTexture(tex_id);
			RGDescriptor uav = texture->CreateAndTakeUAV(&view_desc);
			texture_uav_cache.insert(std::make_pair(res_id, uav));
			return uav;
		}
	}

	RGDescriptor RenderGraph::GetDescriptor(RGBufferReadOnlyId res_id) const
	{
		if (auto it = buffer_srv_cache.find(res_id); it != buffer_srv_cache.end())
		{
			return it->second;
		}
		else
		{
			RGBufferId buf_id = res_id.GetResourceId();
			std::vector<BufferViewDesc>& view_descs = buffer_view_desc_map[buf_id];
			BufferViewDesc const& view_desc = view_descs[res_id.GetViewId()];

			Buffer* buffer = GetBuffer(buf_id);
			RGDescriptor srv = buffer->CreateAndTakeSRV(&view_desc);
			buffer_srv_cache.insert(std::make_pair(res_id, srv));
			return srv;
		}
	}

	RGDescriptor RenderGraph::GetDescriptor(RGBufferReadWriteId res_id) const
	{
		if (auto it = buffer_uav_cache.find(res_id); it != buffer_uav_cache.end())
		{
			return it->second;
		}
		else
		{
			RGBufferId buf_id = res_id.GetResourceId();
			std::vector<BufferViewDesc>& view_descs = buffer_view_desc_map[buf_id];
			BufferViewDesc const& view_desc = view_descs[res_id.GetViewId()];

			Buffer* buffer = GetBuffer(buf_id);
			RGDescriptor uav = buffer->CreateAndTakeUAV(&view_desc);
			buffer_uav_cache.insert(std::make_pair(res_id, uav));
			return uav;
		}
	}

	void RenderGraph::DependencyLevel::AddPass(RenderGraphPassBase* pass)
	{
		passes.push_back(pass);
		texture_reads.insert(pass->texture_reads.begin(), pass->texture_reads.end());
		texture_writes.insert(pass->texture_writes.begin(), pass->texture_writes.end());
		buffer_reads.insert(pass->buffer_reads.begin(), pass->buffer_reads.end());
		buffer_writes.insert(pass->buffer_writes.begin(), pass->buffer_writes.end());
	}

	void RenderGraph::DependencyLevel::Setup()
	{
		for (auto& pass : passes)
		{
			if (pass->IsCulled()) continue;

			texture_creates.insert(pass->texture_creates.begin(), pass->texture_creates.end());
			texture_destroys.insert(pass->texture_destroys.begin(), pass->texture_destroys.end());
			for (auto [resource, state] : pass->texture_state_map)
			{
				texture_state_map[resource] |= state;
			}

			buffer_creates.insert(pass->buffer_creates.begin(), pass->buffer_creates.end());
			buffer_destroys.insert(pass->buffer_destroys.begin(), pass->buffer_destroys.end());
			for (auto [resource, state] : pass->buffer_state_map)
			{
				buffer_state_map[resource] |= state;
			}
		}
	}

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, RGCommandList* cmd_list)
	{
		for (auto& pass : passes)
		{
			if (pass->IsCulled()) continue;
			RenderGraphResources rg_resources(rg, *pass);
			if (pass->type == ERGPassType::Graphics && !pass->SkipAutoRenderPassSetup())
			{
				RenderPassDesc render_pass_desc{};
				if (pass->AllowUAVWrites()) render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
				else render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_NONE;

				for (auto const& render_target_info : pass->render_targets_info)
				{
					RGTextureId rt_texture = render_target_info.render_target_handle.GetResourceId();
					Texture* texture = rg.GetTexture(rt_texture);

					RtvAttachmentDesc rtv_desc{};
					std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
					rtv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
					rtv_desc.cpu_handle = rg.GetDescriptor(render_target_info.render_target_handle);
					
					ERGLoadAccessOp load_access = ERGLoadAccessOp::NoAccess;
					ERGStoreAccessOp store_access = ERGStoreAccessOp::NoAccess;
					SplitAccessOp(render_target_info.render_target_access, load_access, store_access);

					switch (load_access)
					{
					case ERGLoadAccessOp::Clear:
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
						break;
					case ERGLoadAccessOp::Discard:
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
						break;
					case ERGLoadAccessOp::Preserve: 
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
						break;
					case ERGLoadAccessOp::NoAccess:
						rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Load Access!");
					}

					switch (store_access)
					{
					case ERGStoreAccessOp::Resolve:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
						break;
					case ERGStoreAccessOp::Discard:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
						break;
					case ERGStoreAccessOp::Preserve:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
						break;
					case ERGStoreAccessOp::NoAccess:
						rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Store Access!");
					}

					render_pass_desc.rtv_attachments.push_back(std::move(rtv_desc));
				}
				
				if (pass->depth_stencil.has_value())
				{
					auto depth_stencil_info = pass->depth_stencil.value();
					RGTextureId ds_texture = depth_stencil_info.depth_stencil_handle.GetResourceId();
					Texture* texture = rg.GetTexture(ds_texture);

					DsvAttachmentDesc dsv_desc{};
					std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
					dsv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
					dsv_desc.cpu_handle = rg.GetDescriptor(depth_stencil_info.depth_stencil_handle);

					ERGLoadAccessOp load_access = ERGLoadAccessOp::NoAccess;
					ERGStoreAccessOp store_access = ERGStoreAccessOp::NoAccess;
					SplitAccessOp(depth_stencil_info.depth_access, load_access, store_access);

					switch (load_access)
					{
					case ERGLoadAccessOp::Clear:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
						break;
					case ERGLoadAccessOp::Discard:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
						break;
					case ERGLoadAccessOp::Preserve:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
						break;
					case ERGLoadAccessOp::NoAccess:
						dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Load Access!");
					}

					switch (store_access)
					{
					case ERGStoreAccessOp::Resolve:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
						break;
					case ERGStoreAccessOp::Discard:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
						break;
					case ERGStoreAccessOp::Preserve:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
						break;
					case ERGStoreAccessOp::NoAccess:
						dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;;
						break;
					default:
						ADRIA_ASSERT(false && "Invalid Store Access!");
					}
					//todo add stencil
					render_pass_desc.dsv_attachment = std::move(dsv_desc);
				}
				ADRIA_ASSERT((pass->viewport_width != 0 && pass->viewport_height != 0) && "Viewport Width/Height is 0! The call to builder.SetViewport is probably missing...");
				render_pass_desc.width = pass->viewport_width;
				render_pass_desc.height = pass->viewport_height;
				RenderPass render_pass(render_pass_desc);
				PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, pass->name.c_str());
				render_pass.Begin(cmd_list, pass->UseLegacyRenderPasses());
				pass->Execute(rg_resources, gfx, cmd_list);
				render_pass.End(cmd_list, pass->UseLegacyRenderPasses());
			}
			else
			{
				PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, pass->name.c_str());
				pass->Execute(rg_resources, gfx, cmd_list);
			}
		}
	}

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, std::vector<RGCommandList*> const& cmd_lists)
	{
		std::vector<std::future<void>> futures;
		for (size_t i = 0; i < passes.size(); ++i)
		{
			auto& pass = passes[i];
			if (pass->IsCulled()) continue;
			futures.push_back(TaskSystem::Submit([&](size_t j) 
				{
					RGCommandList* cmd_list = cmd_lists[j];
					RenderGraphResources rg_resources(rg, *pass);
					if (pass->type == ERGPassType::Graphics && !pass->SkipAutoRenderPassSetup())
					{
						RenderPassDesc render_pass_desc{};
						if (pass->AllowUAVWrites()) render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
						else render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_NONE;

						for (auto const& render_target_info : pass->render_targets_info)
						{
							RGTextureId rt_texture = render_target_info.render_target_handle.GetResourceId();
							Texture* texture = rg.GetTexture(rt_texture);

							RtvAttachmentDesc rtv_desc{};
							std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
							rtv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
							rtv_desc.cpu_handle = rg.GetDescriptor(render_target_info.render_target_handle);

							ERGLoadAccessOp load_access = ERGLoadAccessOp::NoAccess;
							ERGStoreAccessOp store_access = ERGStoreAccessOp::NoAccess;
							SplitAccessOp(render_target_info.render_target_access, load_access, store_access);

							switch (load_access)
							{
							case ERGLoadAccessOp::Clear:
								rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
								break;
							case ERGLoadAccessOp::Discard:
								rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
								break;
							case ERGLoadAccessOp::Preserve:
								rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
								break;
							case ERGLoadAccessOp::NoAccess:
								rtv_desc.beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
								break;
							default:
								ADRIA_ASSERT(false && "Invalid Load Access!");
							}

							switch (store_access)
							{
							case ERGStoreAccessOp::Resolve:
								rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
								break;
							case ERGStoreAccessOp::Discard:
								rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
								break;
							case ERGStoreAccessOp::Preserve:
								rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
								break;
							case ERGStoreAccessOp::NoAccess:
								rtv_desc.ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;;
								break;
							default:
								ADRIA_ASSERT(false && "Invalid Store Access!");
							}

							render_pass_desc.rtv_attachments.push_back(std::move(rtv_desc));
						}

						if (pass->depth_stencil.has_value())
						{
							auto depth_stencil_info = pass->depth_stencil.value();
							RGTextureId ds_texture = depth_stencil_info.depth_stencil_handle.GetResourceId();
							Texture* texture = rg.GetTexture(ds_texture);

							DsvAttachmentDesc dsv_desc{};
							std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
							dsv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
							dsv_desc.cpu_handle = rg.GetDescriptor(depth_stencil_info.depth_stencil_handle);

							ERGLoadAccessOp load_access = ERGLoadAccessOp::NoAccess;
							ERGStoreAccessOp store_access = ERGStoreAccessOp::NoAccess;
							SplitAccessOp(depth_stencil_info.depth_access, load_access, store_access);

							switch (load_access)
							{
							case ERGLoadAccessOp::Clear:
								dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
								break;
							case ERGLoadAccessOp::Discard:
								dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
								break;
							case ERGLoadAccessOp::Preserve:
								dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
								break;
							case ERGLoadAccessOp::NoAccess:
								dsv_desc.depth_beginning_access = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
								break;
							default:
								ADRIA_ASSERT(false && "Invalid Load Access!");
							}

							switch (store_access)
							{
							case ERGStoreAccessOp::Resolve:
								dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
								break;
							case ERGStoreAccessOp::Discard:
								dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
								break;
							case ERGStoreAccessOp::Preserve:
								dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
								break;
							case ERGStoreAccessOp::NoAccess:
								dsv_desc.depth_ending_access = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;;
								break;
							default:
								ADRIA_ASSERT(false && "Invalid Store Access!");
							}
							//todo add stencil
							render_pass_desc.dsv_attachment = std::move(dsv_desc);
						}
						ADRIA_ASSERT((pass->viewport_width != 0 && pass->viewport_height != 0) && "Viewport Width/Height is 0! The call to builder.SetViewport is probably missing...");
						render_pass_desc.width = pass->viewport_width;
						render_pass_desc.height = pass->viewport_height;
						RenderPass render_pass(render_pass_desc);
						PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, pass->name.c_str());
						render_pass.Begin(cmd_list, pass->UseLegacyRenderPasses());
						pass->Execute(rg_resources, gfx, cmd_list);
						render_pass.End(cmd_list, pass->UseLegacyRenderPasses());
					}
					else
					{
						PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, pass->name.c_str());
						pass->Execute(rg_resources, gfx, cmd_list);
					}
			}, i));
		}

		for (auto& future : futures) future.wait();
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

