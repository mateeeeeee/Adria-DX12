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
	RGTextureId RenderGraph::DeclareTexture(RGResourceName name, RGTextureDesc const& desc)
	{
		TextureDesc tex_desc{}; FillTextureDesc(desc, tex_desc);
		textures.emplace_back(new RGTexture(textures.size(), tex_desc));
		texture_name_id_map[name] = RGTextureId(textures.size() - 1);
		return RGTextureId(textures.size() - 1);
	}

	RGBufferId RenderGraph::DeclareBuffer(RGResourceName name, RGBufferDesc const& desc)
	{
		BufferDesc buf_desc{}; FillBufferDesc(desc, buf_desc);
		buffers.emplace_back(new RGBuffer(buffers.size(), buf_desc));
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

		auto cmd_list = gfx->GetNewGraphicsCommandList();
		for (size_t i = 0; i < dependency_levels.size(); ++i)
		{
			auto& dependency_level = dependency_levels[i];
			for (auto tex_id : dependency_level.texture_creates)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				rg_texture->resource = pool.AllocateTexture(rg_texture->desc);
				CreateTextureViews(tex_id);
			}
			for (auto buf_id : dependency_level.buffer_creates)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				rg_buffer->resource = pool.AllocateBuffer(rg_buffer->desc);
				CreateBufferViews(buf_id);
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
							D3D12_RESOURCE_STATES initial_state = ConvertToD3D12ResourceState(texture->GetDesc().initial_state);
							D3D12_RESOURCE_STATES wanted_state = ConvertToD3D12ResourceState(state);
							barrier_batcher.AddTransition(texture->GetNative(), initial_state, wanted_state);
						}
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.texture_state_map.contains(tex_id))
						{
							D3D12_RESOURCE_STATES wanted_state = ConvertToD3D12ResourceState(state);
							D3D12_RESOURCE_STATES prev_state = ConvertToD3D12ResourceState(prev_dependency_level.texture_state_map[tex_id]);
							if (prev_state != wanted_state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, wanted_state); 
							found = true;
							break;
						}
					}
					if (!found && rg_texture->imported)
					{
						D3D12_RESOURCE_STATES wanted_state = ConvertToD3D12ResourceState(state);
						D3D12_RESOURCE_STATES prev_state = ConvertToD3D12ResourceState(rg_texture->desc.initial_state);
						if (prev_state != wanted_state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, wanted_state); 
					}
				}
				for (auto const& [buf_id, state] : dependency_level.buffer_state_map)
				{
					RGBuffer* rg_buffer = GetRGBuffer(buf_id);
					Buffer* buffer = rg_buffer->resource;
					if (dependency_level.buffer_creates.contains(buf_id))
					{
						if (state != EResourceState::Common) //check if there is an implicit transition, maybe this can be avoided
						{
							barrier_batcher.AddTransition(buffer->GetNative(), D3D12_RESOURCE_STATE_COMMON, ConvertToD3D12ResourceState(state));
						}
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.buffer_state_map.contains(buf_id))
						{
							D3D12_RESOURCE_STATES wanted_state = ConvertToD3D12ResourceState(state);
							D3D12_RESOURCE_STATES prev_state = ConvertToD3D12ResourceState(prev_dependency_level.buffer_state_map[buf_id]);
							if (prev_state != wanted_state) barrier_batcher.AddTransition(buffer->GetNative(), prev_state, wanted_state);
							found = true;
							break;
						}
					}
					if (!found && rg_buffer->imported)
					{
						if (EResourceState::Common != state) barrier_batcher.AddTransition(buffer->GetNative(), D3D12_RESOURCE_STATE_COMMON, ConvertToD3D12ResourceState(state));
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
				EResourceState initial_state = texture->GetDesc().initial_state;
				D3D12_RESOURCE_STATES wanted_state = ConvertToD3D12ResourceState(initial_state);
				ADRIA_ASSERT(dependency_level.texture_state_map.contains(tex_id));
				EResourceState state = dependency_level.texture_state_map[tex_id];
				D3D12_RESOURCE_STATES prev_state = ConvertToD3D12ResourceState(state);
				if (initial_state != state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, wanted_state);
				if (!rg_texture->imported) pool.ReleaseTexture(rg_texture->resource);
			}
			for (RGBufferId buf_id : dependency_level.buffer_destroys)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				Buffer* buffer = rg_buffer->resource;
				D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
				ADRIA_ASSERT(dependency_level.buffer_state_map.contains(buf_id));
				EResourceState state = dependency_level.buffer_state_map[buf_id];
				D3D12_RESOURCE_STATES prev_state = ConvertToD3D12ResourceState(state);
				if (initial_state != prev_state) barrier_batcher.AddTransition(buffer->GetNative(), prev_state, initial_state);
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
			if (textures[i]->last_used_by != nullptr) textures[i]->last_used_by->texture_destroys.insert(RGTextureId(i));
			if (textures[i]->imported) CreateTextureViews(RGTextureId(i));
		}
		for (size_t i = 0; i < buffers.size(); ++i)
		{
			if (buffers[i]->last_used_by != nullptr) buffers[i]->last_used_by->buffer_destroys.insert(RGBufferId(i));
			if (buffers[i]->imported) CreateBufferViews(RGBufferId(i));
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

	void RenderGraph::CreateTextureViews(RGTextureId res_id)
	{
		auto const& view_descs = texture_view_desc_map[res_id];
		for (auto const& [view_desc, type] : view_descs)
		{
			Texture* texture = GetTexture(res_id);
			RGDescriptor view;
			switch (type)
			{
			case ERGDescriptorType::RenderTarget:
				view = texture->TakeSubresource_RTV(&view_desc);
				break;
			case ERGDescriptorType::DepthStencil:
				view = texture->TakeSubresource_DSV(&view_desc);
				break;
			case ERGDescriptorType::ReadOnly:
				view = texture->TakeSubresource_SRV(&view_desc);
				break;
			case ERGDescriptorType::ReadWrite:
				view = texture->TakeSubresource_UAV(&view_desc);
				break;
			default:
				ADRIA_ASSERT(false && "invalid resource view type for texture");
			}
			texture_view_map[res_id].push_back(view);
		}
	}

	void RenderGraph::CreateBufferViews(RGBufferId res_id)
	{
		auto const& view_descs = buffer_view_desc_map[res_id];
		for (auto const& [view_desc, type] : view_descs)
		{
			Buffer* buffer = GetBuffer(res_id);
			RGDescriptor view;
			switch (type)
			{
			case ERGDescriptorType::ReadOnly:
				view = buffer->TakeSubresource_SRV(&view_desc);
				break;
			case ERGDescriptorType::ReadWrite:
				view = buffer->TakeSubresource_UAV(&view_desc);
				break;
			case ERGDescriptorType::RenderTarget:
			case ERGDescriptorType::DepthStencil:
			default:
				ADRIA_ASSERT(false && "invalid resource view type for buffer");
			}
			buffer_view_map[res_id].push_back(view);
		}
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

	void RenderGraph::AddBufferBindFlags(RGResourceName name, EBindFlag flags)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= flags;
	}

	void RenderGraph::AddTextureBindFlags(RGResourceName name, EBindFlag flags)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= flags;
	}

	RGTextureCopySrcId RenderGraph::ReadCopySrcTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		if (rg_texture->desc.initial_state == EResourceState::Common)
		{
			rg_texture->desc.initial_state = EResourceState::CopySource;
		}
		return RGTextureCopySrcId(handle);
	}

	RGTextureCopyDstId RenderGraph::WriteCopyDstTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		if (rg_texture->desc.initial_state == EResourceState::Common)
		{
			rg_texture->desc.initial_state = EResourceState::CopyDest;
		}
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

	RGRenderTargetId RenderGraph::RenderTarget(RGResourceName name, TextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= EBindFlag::RenderTarget;
		if (rg_texture->desc.initial_state == EResourceState::Common)
		{
			rg_texture->desc.initial_state = EResourceState::RenderTarget;
		}
		std::vector<std::pair<TextureSubresourceDesc, ERGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, ERGDescriptorType::RenderTarget);
		return RGRenderTargetId(view_id, handle);
	}

	RGDepthStencilId RenderGraph::DepthStencil(RGResourceName name, TextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= EBindFlag::DepthStencil;
		if (rg_texture->desc.initial_state == EResourceState::Common)
		{
			rg_texture->desc.initial_state = EResourceState::DepthWrite;
		}
		std::vector<std::pair<TextureSubresourceDesc, ERGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, ERGDescriptorType::DepthStencil);
		return RGDepthStencilId(view_id, handle);
	}

	RGTextureReadOnlyId RenderGraph::ReadTexture(RGResourceName name, TextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= EBindFlag::ShaderResource;
		if (rg_texture->desc.initial_state == EResourceState::Common)
		{
			rg_texture->desc.initial_state = EResourceState::PixelShaderResource | EResourceState::NonPixelShaderResource;
		}
		std::vector<std::pair<TextureSubresourceDesc, ERGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, ERGDescriptorType::ReadOnly);
		return RGTextureReadOnlyId(view_id, handle);
	}

	RGTextureReadWriteId RenderGraph::WriteTexture(RGResourceName name, TextureSubresourceDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT(IsValidTextureHandle(handle) && "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= EBindFlag::UnorderedAccess;
		if (rg_texture->desc.initial_state == EResourceState::Common)
		{
			rg_texture->desc.initial_state = EResourceState::UnorderedAccess;
		}
		std::vector<std::pair<TextureSubresourceDesc, ERGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, ERGDescriptorType::ReadWrite);
		return RGTextureReadWriteId(view_id, handle);
	}

	RGBufferReadOnlyId RenderGraph::ReadBuffer(RGResourceName name, BufferSubresourceDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= EBindFlag::ShaderResource;
		std::vector<std::pair<BufferSubresourceDesc, ERGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, ERGDescriptorType::ReadOnly);
		return RGBufferReadOnlyId(view_id, handle);
	}

	RGBufferReadWriteId RenderGraph::WriteBuffer(RGResourceName name, BufferSubresourceDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT(IsValidBufferHandle(handle) && "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= EBindFlag::UnorderedAccess;
		std::vector<std::pair<BufferSubresourceDesc, ERGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.emplace_back(desc, ERGDescriptorType::ReadWrite);
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

	Texture const& RenderGraph::GetCopySrcTexture(RGTextureCopySrcId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	Texture const& RenderGraph::GetCopyDstTexture(RGTextureCopyDstId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	Buffer const& RenderGraph::GetCopySrcBuffer(RGBufferCopySrcId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	Buffer const& RenderGraph::GetCopyDstBuffer(RGBufferCopyDstId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	Buffer const& RenderGraph::GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	RGDescriptor RenderGraph::GetRenderTarget(RGRenderTargetId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		std::vector<RGDescriptor> const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()];
	}

	RGDescriptor RenderGraph::GetDepthStencil(RGDepthStencilId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		std::vector<RGDescriptor> const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()];
	}

	RGDescriptor RenderGraph::GetReadOnlyTexture(RGTextureReadOnlyId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		std::vector<RGDescriptor> const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()];
	}

	RGDescriptor RenderGraph::GetReadWriteTexture(RGTextureReadWriteId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		std::vector<RGDescriptor> const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()];
	}

	RGDescriptor RenderGraph::GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const
	{
		RGBufferId buf_id = res_id.GetResourceId();
		std::vector<RGDescriptor> const& views = buffer_view_map[buf_id];
		return views[res_id.GetViewId()];
	}

	RGDescriptor RenderGraph::GetReadWriteBuffer(RGBufferReadWriteId res_id) const
	{
		RGBufferId buf_id = res_id.GetResourceId();
		std::vector<RGDescriptor> const& views = buffer_view_map[buf_id];
		return views[res_id.GetViewId()];
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

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, CommandList* cmd_list)
	{
		for (auto& pass : passes)
		{
			if (pass->IsCulled()) continue;
			RenderGraphContext rg_resources(rg, *pass);
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
					TextureDesc const& desc = texture->GetDesc();
					ClearValue const& clear_value = desc.clear_value;
					if (clear_value.active_member != ClearValue::ActiveMember::None)
					{
						ADRIA_ASSERT(clear_value.active_member == ClearValue::ActiveMember::Color && "Invalid Clear Value for Render Target");
						rtv_desc.clear_value.Format = desc.format;
						rtv_desc.clear_value.Color[0] = desc.clear_value.color.color[0];
						rtv_desc.clear_value.Color[1] = desc.clear_value.color.color[1];
						rtv_desc.clear_value.Color[2] = desc.clear_value.color.color[2];
						rtv_desc.clear_value.Color[3] = desc.clear_value.color.color[3];
					}
					rtv_desc.cpu_handle = rg.GetRenderTarget(render_target_info.render_target_handle);
					
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
					TextureDesc const& desc = texture->GetDesc();
					ClearValue const& clear_value = desc.clear_value;
					if (clear_value.active_member != ClearValue::ActiveMember::None)
					{
						ADRIA_ASSERT(clear_value.active_member == ClearValue::ActiveMember::DepthStencil && "Invalid Clear Value for Depth Stencil");
						dsv_desc.clear_value.Format = desc.format;
						dsv_desc.clear_value.DepthStencil.Depth = desc.clear_value.depth_stencil.depth;
						dsv_desc.clear_value.DepthStencil.Stencil = desc.clear_value.depth_stencil.stencil;
					}
					dsv_desc.cpu_handle = rg.GetDepthStencil(depth_stencil_info.depth_stencil_handle);

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

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, std::vector<CommandList*> const& cmd_lists)
	{
#ifdef RG_MULTITHREADED
		static_assert(false && "Todo");
#endif
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

