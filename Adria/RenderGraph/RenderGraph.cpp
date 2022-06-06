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

	RGTextureRef RenderGraphBuilder::CreateTexture(char const* name, TextureDesc const& desc)
	{
		RGTextureRef handle = rg.CreateTexture(name, desc);
		rg_pass.creates.insert(handle);
		return handle;
	}

	RGTextureRef RenderGraphBuilder::Read(RGTextureRef handle, ERGReadAccess read_flag /*= ReadFlag_PixelShaderAccess*/)
	{
		if (rg_pass.type == ERGPassType::Copy) read_flag = ReadAccess_CopySrc;

		rg_pass.reads.insert(handle);
		switch (read_flag)
		{
		case ReadAccess_PixelShader:
			rg_pass.resource_state_map[handle] |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			break;
		case ReadAccess_NonPixelShader:
			rg_pass.resource_state_map[handle] |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			break;
		case ReadAccess_AllShader:
			rg_pass.resource_state_map[handle] |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
			break;
		case ReadAccess_CopySrc:
			rg_pass.resource_state_map[handle] |= D3D12_RESOURCE_STATE_COPY_SOURCE;
			break;
		case ReadAccess_IndirectArgument:
			rg_pass.resource_state_map[handle] |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Read Flag!");
		}

		return handle;
	}

	RGTextureRef RenderGraphBuilder::Write(RGTextureRef handle, ERGWriteAccess write_flag /*= WriteFlag_UnorderedAccess*/)
	{
		if (rg_pass.type == ERGPassType::Copy) write_flag = WriteAccess_CopyDst;

		D3D12_RESOURCE_STATES resource_states = D3D12_RESOURCE_STATE_COMMON;
		switch (write_flag)
		{
		case WriteAccess_Unordered:
			resource_states = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			break;
		case WriteAccess_CopyDst:
			resource_states = D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Read Flag!");
		}

		rg_pass.resource_state_map[handle] |= resource_states;
		auto* texture = rg.GetRGTexture(handle);
		if (texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;

		if (!rg_pass.creates.contains(handle))
		{
			++texture->version;
		}
		rg_pass.writes.insert(handle);
		return handle;
	}

	RGTextureRef RenderGraphBuilder::RenderTarget(RGTextureRTVRef rtv_handle, ERGLoadStoreAccessOp load_store_op)
	{
		RGTextureRef handle = rtv_handle.GetResourceHandle();
		rg_pass.resource_state_map[handle] |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		rg_pass.render_targets_info.push_back(RenderGraphPassBase::RenderTargetInfo{ .render_target_handle = rtv_handle, .render_target_access = load_store_op });
		auto* rg_texture = rg.GetRGTexture(handle);
		if (rg_texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;

		if (!rg_pass.creates.contains(handle))
		{
			++rg_texture->version;
		}
		rg_pass.writes.insert(handle);
		return handle;
	}

	RGTextureRef RenderGraphBuilder::DepthStencil(RGTextureDSVRef dsv_handle, ERGLoadStoreAccessOp depth_load_store_op, bool readonly /*= false*/, ERGLoadStoreAccessOp stencil_load_store_op /*= ERGLoadStoreAccessOp::NoAccess_NoAccess*/)
	{
		RGTextureRef handle = dsv_handle.GetResourceHandle();
		rg_pass.depth_stencil = RenderGraphPassBase::DepthStencilInfo{ .depth_stencil_handle = dsv_handle, .depth_access = depth_load_store_op,.stencil_access = stencil_load_store_op, .readonly = readonly };
		auto* rg_texture = rg.GetRGTexture(handle);

		if (!rg_pass.creates.contains(handle) && !readonly)
		{
			++rg_texture->version;
		}
		readonly ? rg_pass.reads.insert(handle) : rg_pass.writes.insert(handle);

		if (rg_texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		rg_pass.resource_state_map[handle] |= readonly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
		return handle;
	}

	RGTextureSRVRef RenderGraphBuilder::CreateSRV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		return rg.CreateSRV(handle, desc);
	}

	RGTextureUAVRef RenderGraphBuilder::CreateUAV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		return rg.CreateUAV(handle, desc);
	}

	RGTextureRTVRef RenderGraphBuilder::CreateRTV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		return rg.CreateRTV(handle, desc);
	}

	RGTextureDSVRef RenderGraphBuilder::CreateDSV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		return rg.CreateDSV(handle, desc);
	}

	void RenderGraphBuilder::SetViewport(uint32 width, uint32 height)
	{
		rg_pass.viewport_width = width;
		rg_pass.viewport_height = height;
	}

	RenderGraphBuilder::RenderGraphBuilder(RenderGraph& rg, RenderGraphPassBase& rg_pass)
		: rg(rg), rg_pass(rg_pass)
	{}

	RGTextureRef RenderGraph::CreateTexture(char const* name, TextureDesc const& desc)
	{
		textures.emplace_back(new RGTexture(name, textures.size(), desc));
		return RGTextureRef(textures.size() - 1);
	}

	RGTextureRef RenderGraph::ImportTexture(char const* name, Texture* texture)
	{
		textures.emplace_back(new RGTexture(name, textures.size(), texture));
		return RGTextureRef(textures.size() - 1);
	}

	Texture* RenderGraph::GetTexture(RGTextureRef handle) const
	{
		RGTexture* texture = GetRGTexture(handle);
		return texture->resource;
	}

	RGBufferRef RenderGraph::CreateBuffer(char const* name, BufferDesc const& desc)
	{
		return RGBufferRef{};
	}

	Buffer* RenderGraph::GetBuffer(RGBufferRef handle) const
	{
		RGBuffer* buffer = GetRGBuffer(handle);
		return buffer->resource;
	}

	bool RenderGraph::IsValidTextureHandle(RGTextureRef handle) const
	{
		return handle.IsValid() && handle.id < textures.size();
	}

	bool RenderGraph::IsValidBufferHandle(RGBufferRef handle) const
	{
		return handle.IsValid() && handle.id < buffers.size();
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
		return Multithreaded ? Execute_Multithreaded() : Execute_Singlethreaded();
	}

	void RenderGraph::Execute_Singlethreaded()
	{
		pool.Tick();

		for (auto& dependency_level : dependency_levels) dependency_level.Setup();

		auto cmd_list = gfx->GetNewGraphicsCommandList();
		ResourceBarrierBatch final_barrier_batcher{};
		for (size_t i = 0; i < dependency_levels.size(); ++i)
		{
			auto& dependency_level = dependency_levels[i];
			for (auto handle : dependency_level.creates)
			{
				RGTexture* rg_texture = GetRGTexture(handle);
				if(!rg_texture->imported) rg_texture->resource = pool.AllocateTexture(rg_texture->desc, ConvertToWide(rg_texture->name).c_str());
			}

			ResourceBarrierBatch barrier_batcher{};
			{
				for (auto const& [texture_handle, state] : dependency_level.required_states)
				{
					RGTexture* rg_texture = GetRGTexture(texture_handle);
					Texture* texture = rg_texture->resource;
					if (dependency_level.creates.contains(texture_handle))
					{
						ADRIA_ASSERT(HasAllFlags(texture->GetDesc().initial_state, state) && "First Resource Usage must be compatible with initial resource state!");
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.required_states.contains(texture_handle))
						{
							ResourceState prev_state = prev_dependency_level.required_states[texture_handle];
							if (state != prev_state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, state);
							found = true;
							break;
						}
					}
					if (!found && rg_texture->imported)
					{
						ResourceState prev_state = rg_texture->desc.initial_state;
						if (state != prev_state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, state);
					}
				}
			}

			barrier_batcher.Submit(cmd_list);
			dependency_level.Execute(gfx, cmd_list);
			for (auto handle : dependency_level.destroys)
			{
				auto* rg_texture = GetRGTexture(handle);
				Texture* texture = rg_texture->resource;
				ResourceState initial_state = texture->GetDesc().initial_state;
				ADRIA_ASSERT(dependency_level.required_states.contains(handle));
				ResourceState state = dependency_level.required_states[handle];
				if (initial_state != state) final_barrier_batcher.AddTransition(texture->GetNative(), state, initial_state);
				if (!rg_texture->imported) pool.ReleaseTexture(rg_texture->resource);
			}
		}
		final_barrier_batcher.Submit(cmd_list);
	}

	void RenderGraph::Execute_Multithreaded()
	{
		pool.Tick();

		for (auto& dependency_level : dependency_levels) dependency_level.Setup();

		ResourceBarrierBatch final_barrier_batcher{};
		for (size_t i = 0; i < dependency_levels.size(); ++i)
		{
			auto& dependency_level = dependency_levels[i];
			for (auto handle : dependency_level.creates)
			{
				RGTexture* rg_texture = GetRGTexture(handle);
				if (!rg_texture->imported) rg_texture->resource = pool.AllocateTexture(rg_texture->desc, ConvertToWide(rg_texture->name).c_str());
			}

			ResourceBarrierBatch barrier_batcher{};
			{
				for (auto const& [texture_handle, state] : dependency_level.required_states)
				{
					RGTexture* rg_texture = GetRGTexture(texture_handle);
					Texture* texture = rg_texture->resource;
					if (dependency_level.creates.contains(texture_handle))
					{
						ADRIA_ASSERT(HasAllFlags(texture->GetDesc().initial_state, state) && "First Resource Usage must be compatible with initial resource state!");
						continue;
					}
					bool found = false;
					for (int32 j = (int32)i - 1; j >= 0; --j)
					{
						auto& prev_dependency_level = dependency_levels[j];
						if (prev_dependency_level.required_states.contains(texture_handle))
						{
							ResourceState prev_state = prev_dependency_level.required_states[texture_handle];
							if (state != prev_state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, state);
							found = true;
							break;
						}
					}
					if (!found && rg_texture->imported)
					{
						ResourceState prev_state = rg_texture->desc.initial_state;
						if (state != prev_state) barrier_batcher.AddTransition(texture->GetNative(), prev_state, state);
					}
				}
			}

			std::vector<CommandList*> cmd_lists(dependency_level.GetNonCulledSize());
			for (size_t i = 0; i < cmd_lists.size(); ++i) cmd_lists[i] = gfx->GetNewGraphicsCommandList();

			barrier_batcher.Submit(cmd_lists.front());
			dependency_level.Execute(gfx, cmd_lists);
			for (auto handle : dependency_level.destroys)
			{
				auto* rg_texture = GetRGTexture(handle);
				Texture* texture = rg_texture->resource;
				ResourceState initial_state = texture->GetDesc().initial_state;
				ADRIA_ASSERT(dependency_level.required_states.contains(handle));
				ResourceState state = dependency_level.required_states[handle];
				if (initial_state != state) final_barrier_batcher.AddTransition(texture->GetNative(), state, initial_state);
				if (!rg_texture->imported) pool.ReleaseTexture(rg_texture->resource);
			}
		}
		auto cmd_list = gfx->GetNewGraphicsCommandList();
		final_barrier_batcher.Submit(cmd_list);
	}

	RGTexture* RenderGraph::GetRGTexture(RGTextureRef handle) const
	{
		return textures[handle.id].get();
	}

	RGBuffer* RenderGraph::GetRGBuffer(RGBufferRef handle) const
	{
		return buffers[handle.id].get();
	}

	void RenderGraph::BuildAdjacencyLists()
	{
		adjacency_lists.resize(passes.size());
		for (size_t i = 0; i < passes.size(); ++i)
		{
			auto& pass = passes[i];
			std::vector<uint64>& pass_adjacency_list = adjacency_lists[i];
			for (size_t j = passes.size(); j-- != 0;)
			{
				if (i == j) continue;

				auto& other_pass = passes[j];
				for (auto const& other_node_read : other_pass->reads)
				{
					bool depends = pass->writes.find(other_node_read) != pass->writes.end();
					if (depends)
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
				if (distances[v] < distances[u] + 1) distances[v] = distances[u] + 1;
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
			pass->ref_count = pass->writes.size();
			for (auto id : pass->reads)
			{
				auto* consumed = GetRGTexture(id);
				++consumed->ref_count;
			}
			for (auto id : pass->writes)
			{
				auto* written = GetRGTexture(id);
				written->writer = pass.get();
			}
		}

		std::stack<RGTexture*> zero_ref_resources;
		for (auto& texture : textures) if (texture->ref_count == 0) zero_ref_resources.push(texture.get());

		while (!zero_ref_resources.empty())
		{
			RGTexture* unreferenced_resource = zero_ref_resources.top();
			zero_ref_resources.pop();
			auto* writer = unreferenced_resource->writer;
			if (writer == nullptr || !writer->CanBeCulled()) continue;

			if (--writer->ref_count == 0)
			{
				for (auto id : writer->reads)
				{
					auto* texture = GetRGTexture(id);
					if (--texture->ref_count == 0) zero_ref_resources.push(texture);
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
			{
				RGTexture* rg_texture = GetRGTexture(id);
				rg_texture->last_used_by = pass.get();
			}
				
			for (auto id : pass->reads)
			{
				RGTexture* rg_texture = GetRGTexture(id);
				rg_texture->last_used_by = pass.get();
			}
		}

		for (size_t i = 0; i < textures.size(); ++i)
		{
			if(textures[i]->last_used_by != nullptr) textures[i]->last_used_by->destroy.insert(RGTextureRef(i));
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

	RGTextureSRVRef RenderGraph::CreateSRV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		//add check if desc already exists?
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureSRVRef(view_id, handle);
	}

	RGTextureUAVRef RenderGraph::CreateUAV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureUAVRef(view_id, handle);
	}

	RGTextureRTVRef RenderGraph::CreateRTV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureRTVRef(view_id, handle);
	}

	RGTextureDSVRef RenderGraph::CreateDSV(RGTextureRef handle, TextureViewDesc const& desc)
	{
		std::vector<TextureViewDesc>& view_descs = view_desc_map[handle];
		size_t view_id = view_descs.size();
		view_descs.push_back(desc);
		return RGTextureDSVRef(view_id, handle);
	}

	ResourceView RenderGraph::GetSRV(RGTextureSRVRef handle) const
	{
		//std::scoped_lock lock(srv_cache_mutex);
		if (auto it = texture_srv_cache.find(handle); it != texture_srv_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureRef tex_handle = handle.GetResourceHandle();
			std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
			TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

			Texture* texture = GetTexture(tex_handle);
			ResourceView srv = texture->CreateAndTakeSRV(&view_desc);
			texture_srv_cache.insert(std::make_pair(handle, srv));
			return srv;
		}
	}

	ResourceView RenderGraph::GetUAV(RGTextureUAVRef handle) const
	{
		//std::scoped_lock lock(uav_cache_mutex);
		if (auto it = texture_uav_cache.find(handle); it != texture_uav_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureRef tex_handle = handle.GetResourceHandle();
			std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
			TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

			Texture* texture = GetTexture(tex_handle);
			ResourceView uav = texture->CreateAndTakeUAV(&view_desc);
			texture_uav_cache.insert(std::make_pair(handle, uav));
			return uav;
		}
	}

	ResourceView RenderGraph::GetRTV(RGTextureRTVRef handle) const
	{
		//std::scoped_lock lock(rtv_cache_mutex);
		if (auto it = texture_rtv_cache.find(handle); it != texture_rtv_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureRef tex_handle = handle.GetResourceHandle();
			std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
			TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

			Texture* texture = GetTexture(tex_handle);
			ResourceView rtv = texture->CreateAndTakeRTV(&view_desc);
			texture_rtv_cache.insert(std::make_pair(handle, rtv));
			return rtv;
		}
	}

	ResourceView RenderGraph::GetDSV(RGTextureDSVRef handle) const
	{
		//std::scoped_lock lock(dsv_cache_mutex);
		if (auto it = texture_dsv_cache.find(handle); it != texture_dsv_cache.end())
		{
			return it->second;
		}
		else
		{
			RGTextureRef tex_handle = handle.GetResourceHandle();
			std::vector<TextureViewDesc>& view_descs = view_desc_map[tex_handle];
			TextureViewDesc const& view_desc = view_descs[handle.GetViewId()];

			Texture* texture = GetTexture(tex_handle);
			ResourceView dsv = texture->CreateAndTakeDSV(&view_desc);
			texture_dsv_cache.insert(std::make_pair(handle, dsv));
			return dsv;
		}
	}

	void RenderGraph::DependencyLevel::AddPass(RenderGraphPassBase* pass)
	{
		passes.push_back(pass);
		reads.insert(pass->reads.begin(), pass->reads.end());
		writes.insert(pass->writes.begin(), pass->writes.end());
	}

	void RenderGraph::DependencyLevel::Setup()
	{
		for (auto& pass : passes)
		{
			if (pass->IsCulled()) continue;
			creates.insert(pass->creates.begin(), pass->creates.end());
			destroys.insert(pass->destroy.begin(), pass->destroy.end());
			for (auto [resource, state] : pass->resource_state_map)
			{
				required_states[resource] |= state;
			}
		}
	}

	void RenderGraph::DependencyLevel::Execute(GraphicsDevice* gfx, CommandList* cmd_list)
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
					RGTextureRef rt_texture = render_target_info.render_target_handle.GetResourceHandle();
					Texture* texture = rg.GetTexture(rt_texture);

					RtvAttachmentDesc rtv_desc{};
					std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
					rtv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
					rtv_desc.cpu_handle = rg.GetRTV(render_target_info.render_target_handle);
					
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
					RGTextureRef ds_texture = depth_stencil_info.depth_stencil_handle.GetResourceHandle();
					Texture* texture = rg.GetTexture(ds_texture);

					DsvAttachmentDesc dsv_desc{};
					std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
					dsv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
					dsv_desc.cpu_handle = rg.GetDSV(depth_stencil_info.depth_stencil_handle);

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
		std::vector<std::future<void>> futures;
		for (size_t i = 0; i < passes.size(); ++i)
		{
			auto& pass = passes[i];
			if (pass->IsCulled()) continue;
			futures.push_back(TaskSystem::Submit([&](size_t j) 
				{
					CommandList* cmd_list = cmd_lists[j];
					RenderGraphResources rg_resources(rg, *pass);
					if (pass->type == ERGPassType::Graphics && !pass->SkipAutoRenderPassSetup())
					{
						RenderPassDesc render_pass_desc{};
						if (pass->AllowUAVWrites()) render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
						else render_pass_desc.render_pass_flags = D3D12_RENDER_PASS_FLAG_NONE;

						for (auto const& render_target_info : pass->render_targets_info)
						{
							RGTextureRef rt_texture = render_target_info.render_target_handle.GetResourceHandle();
							Texture* texture = rg.GetTexture(rt_texture);

							RtvAttachmentDesc rtv_desc{};
							std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
							rtv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
							rtv_desc.cpu_handle = rg.GetRTV(render_target_info.render_target_handle);

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
							RGTextureRef ds_texture = depth_stencil_info.depth_stencil_handle.GetResourceHandle();
							Texture* texture = rg.GetTexture(ds_texture);

							DsvAttachmentDesc dsv_desc{};
							std::optional<D3D12_CLEAR_VALUE> clear = texture->GetDesc().clear;
							dsv_desc.clear_value = clear.has_value() ? clear.value() : D3D12_CLEAR_VALUE{};
							dsv_desc.cpu_handle = rg.GetDSV(depth_stencil_info.depth_stencil_handle);

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

