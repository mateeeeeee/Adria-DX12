#include <stack>
#include <format>
#include <fstream>
#include <pix3.h>
#include "RenderGraph.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxRenderPass.h"
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxTracyProfiler.h"
#include "Utilities/StringUtil.h"
#include "Utilities/FilesUtil.h"
#include "Core/Paths.h"
#include "Logging/Logger.h"


#if GFX_MULTITHREADED
#define RG_MULTITHREADED 1
#else
#define RG_MULTITHREADED 0
#endif

namespace adria
{
	extern bool dump_render_graph = false;

	RGTextureId RenderGraph::DeclareTexture(RGResourceName name, RGTextureDesc const& desc)
	{
		ADRIA_ASSERT_MSG(texture_name_id_map.find(name) == texture_name_id_map.end(), "Texture with that name has already been declared");
		GfxTextureDesc tex_desc{}; InitGfxTextureDesc(desc, tex_desc);
		textures.emplace_back(new RGTexture(textures.size(), tex_desc, name));
		texture_name_id_map[name] = RGTextureId(textures.size() - 1);
		return RGTextureId(textures.size() - 1);
	}

	RGBufferId RenderGraph::DeclareBuffer(RGResourceName name, RGBufferDesc const& desc)
	{
		ADRIA_ASSERT_MSG(buffer_name_id_map.find(name) == buffer_name_id_map.end(), "Buffer with that name has already been declared");
		GfxBufferDesc buf_desc{}; InitGfxBufferDesc(desc, buf_desc);
		buffers.emplace_back(new RGBuffer(buffers.size(), buf_desc, name));
		buffer_name_id_map[name] = RGBufferId(buffers.size() - 1);
		return RGBufferId(buffers.size() - 1);
	}

	bool RenderGraph::IsTextureDeclared(RGResourceName name)
	{
		return texture_name_id_map.contains(name);
	}

	bool RenderGraph::IsBufferDeclared(RGResourceName name)
	{
		return buffer_name_id_map.contains(name);
	}

	void RenderGraph::ImportTexture(RGResourceName name, GfxTexture* texture)
	{
		ADRIA_ASSERT(texture);
		textures.emplace_back(new RGTexture(textures.size(), texture, name));
		textures.back()->SetName();
		texture_name_id_map[name] = RGTextureId(textures.size() - 1);
	}

	void RenderGraph::ImportBuffer(RGResourceName name, GfxBuffer* buffer)
	{
		ADRIA_ASSERT(buffer);
		buffers.emplace_back(new RGBuffer(buffers.size(), buffer, name));
		buffers.back()->SetName();
		buffer_name_id_map[name] = RGBufferId(buffers.size() - 1);
	}

	void RenderGraph::ExportTexture(RGResourceName name, GfxTexture* texture)
	{
		ADRIA_ASSERT_MSG(texture, "Cannot export to a null resource");
		AddExportTextureCopyPass(name, texture);
	}

	void RenderGraph::ExportBuffer(RGResourceName name, GfxBuffer* buffer)
	{
		ADRIA_ASSERT_MSG(buffer, "Cannot export to a null resource");
		AddExportBufferCopyPass(name, buffer);
	}

	bool RenderGraph::IsValidTextureHandle(RGTextureId handle) const
	{
		return handle.IsValid() && handle.id < textures.size();
	}

	bool RenderGraph::IsValidBufferHandle(RGBufferId handle) const
	{
		return handle.IsValid() && handle.id < buffers.size();
	}

	RenderGraph::~RenderGraph()
	{
		for (auto& [tex_id, view_vector] : texture_view_map)
		{
			for (auto [view, type] : view_vector)
			{
				switch (type)
				{
				case RGDescriptorType::RenderTarget:
					gfx->FreeDescriptorCPU(view, GfxDescriptorHeapType::RTV);
					continue;
				case RGDescriptorType::DepthStencil:
					gfx->FreeDescriptorCPU(view, GfxDescriptorHeapType::DSV);
					continue;
				case RGDescriptorType::ReadWrite:
				case RGDescriptorType::ReadOnly:
				default:
					gfx->FreeDescriptorCPU(view, GfxDescriptorHeapType::CBV_SRV_UAV);
				}
			}
		}

		for (auto& [buf_id, view_vector] : buffer_view_map)
		{
			for (auto [view, type] : view_vector) gfx->FreeDescriptorCPU(view, GfxDescriptorHeapType::CBV_SRV_UAV);
		}
	}

	void RenderGraph::Build()
	{
		BuildAdjacencyLists();
		TopologicalSort();
		BuildDependencyLevels();
		CullPasses();
		CalculateResourcesLifetime();
		for (auto& dependency_level : dependency_levels) dependency_level.Setup();
		if (dump_render_graph) Dump("rendergraph.gv");
	}

	void RenderGraph::Execute()
	{
#if RG_MULTITHREADED
		Execute_Multithreaded();
#else
		Execute_Singlethreaded();
#endif
	}

	void RenderGraph::Execute_Singlethreaded()
	{
		pool.Tick();

		GfxCommandList* cmd_list = gfx->GetCommandList();
		for (uint64 i = 0; i < dependency_levels.size(); ++i)
		{
			auto& dependency_level = dependency_levels[i];
			for (auto tex_id : dependency_level.texture_creates)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				rg_texture->resource = pool.AllocateTexture(rg_texture->desc);
				CreateTextureViews(tex_id);
				rg_texture->SetName();
			}
			for (auto buf_id : dependency_level.buffer_creates)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				rg_buffer->resource = pool.AllocateBuffer(rg_buffer->desc);
				CreateBufferViews(buf_id);
				rg_buffer->SetName();
			}
			for (auto const& [tex_id, state] : dependency_level.texture_state_map)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				GfxTexture* texture = rg_texture->resource;
				if (dependency_level.texture_creates.contains(tex_id))
				{
					if (!HasAllFlags(texture->GetDesc().initial_state, state))
					{
						cmd_list->TextureBarrier(*texture, texture->GetDesc().initial_state, state);
					}
					continue;
				}
				bool found = false;
				for (int32 j = (int32)i - 1; j >= 0; --j)
				{
					auto& prev_dependency_level = dependency_levels[j];
					if (prev_dependency_level.texture_state_map.contains(tex_id))
					{
						GfxResourceState prev_state = prev_dependency_level.texture_state_map[tex_id];
						if (prev_state != state) cmd_list->TextureBarrier(*texture, prev_state, state);
						found = true;
						break;
					}
				}
				if (!found && rg_texture->imported)
				{
					GfxResourceState prev_state = rg_texture->desc.initial_state;
					if (prev_state != state) cmd_list->TextureBarrier(*texture, prev_state, state);
				}
			}
			for (auto const& [buf_id, state] : dependency_level.buffer_state_map)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				GfxBuffer* buffer = rg_buffer->resource;
				if (dependency_level.buffer_creates.contains(buf_id))
				{
					if (state != GfxResourceState::Common)
					{
						cmd_list->BufferBarrier(*buffer, GfxResourceState::Common, state);
					}
					continue;
				}
				bool found = false;
				for (int32 j = (int32)i - 1; j >= 0; --j)
				{
					auto& prev_dependency_level = dependency_levels[j];
					if (prev_dependency_level.buffer_state_map.contains(buf_id))
					{
						GfxResourceState prev_state = prev_dependency_level.buffer_state_map[buf_id];
						if (prev_state != state) cmd_list->BufferBarrier(*buffer, prev_state, state);
						found = true;
						break;
					}
				}
				if (!found && rg_buffer->imported)
				{
					if (GfxResourceState::Common != state) cmd_list->BufferBarrier(*buffer, GfxResourceState::Common, state);
				}
			}

			cmd_list->FlushBarriers();
			dependency_level.Execute(gfx, cmd_list);

			for (RGTextureId tex_id : dependency_level.texture_destroys)
			{
				RGTexture* rg_texture = GetRGTexture(tex_id);
				GfxTexture* texture = rg_texture->resource;
				GfxResourceState initial_state = texture->GetDesc().initial_state;
				ADRIA_ASSERT(dependency_level.texture_state_map.contains(tex_id));
				GfxResourceState state = dependency_level.texture_state_map[tex_id];
				if (initial_state != state) cmd_list->TextureBarrier(*texture, state, initial_state);
				if (!rg_texture->imported) pool.ReleaseTexture(rg_texture->resource);
			}
			for (RGBufferId buf_id : dependency_level.buffer_destroys)
			{
				RGBuffer* rg_buffer = GetRGBuffer(buf_id);
				GfxBuffer* buffer = rg_buffer->resource;
				ADRIA_ASSERT(dependency_level.buffer_state_map.contains(buf_id));
				GfxResourceState state = dependency_level.buffer_state_map[buf_id];
				if(state != GfxResourceState::Common) cmd_list->BufferBarrier(*buffer, state, GfxResourceState::Common);
				if (!rg_buffer->imported) pool.ReleaseBuffer(rg_buffer->resource);
			}
			cmd_list->FlushBarriers();
		}
	}

	void RenderGraph::Execute_Multithreaded()
	{
		ADRIA_ASSERT_MSG(false, "Not implemented!");
	}

	void RenderGraph::AddExportBufferCopyPass(RGResourceName export_buffer, GfxBuffer* buffer)
	{
		struct ExportBufferCopyPassData
		{
			RGBufferCopySrcId src;
		};
		AddPass<ExportBufferCopyPassData>("Export Buffer Copy Pass",
			[=](ExportBufferCopyPassData& data, RenderGraphBuilder& builder)
			{
				ADRIA_ASSERT(IsBufferDeclared(export_buffer));
				data.src = builder.ReadCopySrcBuffer(export_buffer);
			},
			[=](ExportBufferCopyPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxBuffer const& src_buffer = context.GetCopySrcBuffer(data.src);
				cmd_list->CopyBuffer(*buffer, src_buffer);
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);
	}

	void RenderGraph::AddExportTextureCopyPass(RGResourceName export_texture, GfxTexture* texture)
	{
		struct ExportTextureCopyPassData
		{
			RGTextureCopySrcId src;
		};

		AddPass<ExportTextureCopyPassData>("Export Texture Copy Pass",
			[=](ExportTextureCopyPassData& data, RenderGraphBuilder& builder)
			{
				ADRIA_ASSERT(IsTextureDeclared(export_texture));
				data.src = builder.ReadCopySrcTexture(export_texture);
			},
			[=](ExportTextureCopyPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxTexture const& src_texture = context.GetCopySrcTexture(data.src);
				cmd_list->CopyTexture(*texture, src_texture);
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);
	}

	void RenderGraph::BuildAdjacencyLists()
	{
		adjacency_lists.resize(passes.size());
		for (uint64 i = 0; i < passes.size(); ++i)
		{
			auto& pass = passes[i];
			std::vector<uint64>& pass_adjacency_list = adjacency_lists[i];
			for (uint64 j = i + 1; j < passes.size(); ++j)
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
		std::vector<uint64> sort{};
		std::vector<bool>  visited(passes.size(), false);
		for (uint64 i = 0; i < passes.size(); i++)
		{
			if (visited[i] == false) DepthFirstSearch(i, visited, topologically_sorted_passes);
		}
		std::reverse(topologically_sorted_passes.begin(), topologically_sorted_passes.end());
	}

	void RenderGraph::BuildDependencyLevels()
	{
		std::vector<uint64> distances(topologically_sorted_passes.size(), 0);
		for (uint64 u = 0; u < topologically_sorted_passes.size(); ++u)
		{
			uint64 i = topologically_sorted_passes[u];
			for (auto v : adjacency_lists[i])
			{
				if (distances[v] < distances[i] + 1) distances[v] = distances[i] + 1;
			}
		}

		dependency_levels.resize(*std::max_element(std::begin(distances), std::end(distances)) + 1, DependencyLevel(*this));
		for (uint64 i = 0; i < passes.size(); ++i)
		{
			uint64 level = distances[i];
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
		for (auto& dependency_level : dependency_levels)
		{
			for (auto& pass : dependency_level.passes)
			{
				if (pass->IsCulled()) continue;
				for (auto id : pass->texture_writes)
				{
					if (!pass->texture_state_map.contains(id)) continue;
					RGTexture* rg_texture = GetRGTexture(id);
					rg_texture->last_used_by = pass;
				}
				for (auto id : pass->buffer_writes)
				{
					if (!pass->buffer_state_map.contains(id)) continue;
					RGBuffer* rg_buffer = GetRGBuffer(id);
					rg_buffer->last_used_by = pass;
				}

				for (auto id : pass->texture_reads)
				{
					if (!pass->texture_state_map.contains(id)) continue;
					RGTexture* rg_texture = GetRGTexture(id);
					rg_texture->last_used_by = pass;
				}
				for (auto id : pass->buffer_reads)
				{
					if (!pass->buffer_state_map.contains(id)) continue;
					RGBuffer* rg_buffer = GetRGBuffer(id);
					rg_buffer->last_used_by = pass;
				}
			}
		}

		for (uint64 i = 0; i < textures.size(); ++i)
		{
			if (textures[i]->last_used_by != nullptr) textures[i]->last_used_by->texture_destroys.insert(RGTextureId(i));
			if (textures[i]->imported) CreateTextureViews(RGTextureId(i));
		}
		for (uint64 i = 0; i < buffers.size(); ++i)
		{
			if (buffers[i]->last_used_by != nullptr) buffers[i]->last_used_by->buffer_destroys.insert(RGBufferId(i));
			if (buffers[i]->imported) CreateBufferViews(RGBufferId(i));
		}
	}

	void RenderGraph::DepthFirstSearch(uint64 i, std::vector<bool>& visited, std::vector<uint64>& topologically_sorted_passes)
	{
		visited[i] = true;
		for (auto j : adjacency_lists[i])
		{
			if (!visited[j]) DepthFirstSearch(j, visited, topologically_sorted_passes);
		}
		topologically_sorted_passes.push_back(i);
	}

	RGTexture* RenderGraph::GetRGTexture(RGTextureId handle) const
	{
		return textures[handle.id].get();
	}

	RGBuffer* RenderGraph::GetRGBuffer(RGBufferId handle) const
	{
		return buffers[handle.id].get();
	}

	GfxTexture* RenderGraph::GetTexture(RGTextureId res_id) const
	{
		return GetRGTexture(res_id)->resource;
	}

	GfxBuffer* RenderGraph::GetBuffer(RGBufferId res_id) const
	{
		return GetRGBuffer(res_id)->resource;
	}

	void RenderGraph::CreateTextureViews(RGTextureId res_id)
	{
		auto const& view_descs = texture_view_desc_map[res_id];
		for (auto const& [view_desc, type] : view_descs)
		{
			GfxTexture* texture = GetTexture(res_id);
			GfxDescriptor view;
			switch (type)
			{
			case RGDescriptorType::RenderTarget:
				view = gfx->CreateTextureRTV(texture, &view_desc);
				break;
			case RGDescriptorType::DepthStencil:
				view = gfx->CreateTextureDSV(texture, &view_desc);
				break;
			case RGDescriptorType::ReadOnly:
				view = gfx->CreateTextureSRV(texture, &view_desc);
				break;
			case RGDescriptorType::ReadWrite:
				view = gfx->CreateTextureUAV(texture, &view_desc);
				break;
			default:
				ADRIA_ASSERT_MSG(false, "invalid resource view type for texture");
			}
			texture_view_map[res_id].emplace_back(view, type);
		}
	}

	void RenderGraph::CreateBufferViews(RGBufferId res_id)
	{
		auto const& view_descs = buffer_view_desc_map[res_id];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [view_desc, type] = view_descs[i];
			GfxBuffer* buffer = GetBuffer(res_id);
			GfxDescriptor view;
			switch (type)
			{
			case RGDescriptorType::ReadOnly:
			{
				view = gfx->CreateBufferSRV(buffer, &view_desc);
				break;
			}
			case RGDescriptorType::ReadWrite:
			{
				RGBufferReadWriteId rw_id(i, res_id);
				if (buffer_uav_counter_map.contains(rw_id))
				{
					GfxBuffer* counter_buffer = GetBuffer(buffer_uav_counter_map[rw_id]);
					view = gfx->CreateBufferUAV(buffer, counter_buffer, &view_desc);
				}
				else view = gfx->CreateBufferUAV(buffer, &view_desc);
				break;
			}
			case RGDescriptorType::RenderTarget:
			case RGDescriptorType::DepthStencil:
			default:
				ADRIA_ASSERT_MSG(false, "invalid resource view type for buffer");
			}
			buffer_view_map[res_id].emplace_back(view, type);
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

	RGTextureDesc RenderGraph::GetTextureDesc(RGResourceName name)
	{
		ADRIA_ASSERT(IsTextureDeclared(name));
		RGTextureId tex_id = texture_name_id_map[name];
		RGTextureDesc desc{};
		ExtractRGTextureDesc(GetRGTexture(tex_id)->desc, desc);
		return desc;
	}

	RGBufferDesc RenderGraph::GetBufferDesc(RGResourceName name)
	{
		ADRIA_ASSERT(IsBufferDeclared(name));
		RGBufferId buf_id = buffer_name_id_map[name];
		RGBufferDesc desc{};
		ExtractRGBufferDesc(GetRGBuffer(buf_id)->desc, desc);
		return desc;
	}

	void RenderGraph::AddBufferBindFlags(RGResourceName name, GfxBindFlag flags)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= flags;
	}

	void RenderGraph::AddTextureBindFlags(RGResourceName name, GfxBindFlag flags)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidTextureHandle(handle), "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= flags;
	}

	RGTextureCopySrcId RenderGraph::ReadCopySrcTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidTextureHandle(handle), "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::CopyDst;
		}
		return RGTextureCopySrcId(handle);
	}

	RGTextureCopyDstId RenderGraph::WriteCopyDstTexture(RGResourceName name)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidTextureHandle(handle), "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::CopyDst;
		}
		return RGTextureCopyDstId(handle);
	}

	RGBufferCopySrcId RenderGraph::ReadCopySrcBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		return RGBufferCopySrcId(handle);
	}

	RGBufferCopyDstId RenderGraph::WriteCopyDstBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		return RGBufferCopyDstId(handle);
	}

	RGBufferIndirectArgsId RenderGraph::ReadIndirectArgsBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		return RGBufferIndirectArgsId(handle);
	}

	RGBufferVertexId RenderGraph::ReadVertexBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		return RGBufferVertexId(handle);
	}

	RGBufferIndexId RenderGraph::ReadIndexBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		return RGBufferIndexId(handle);
	}

	RGBufferConstantId RenderGraph::ReadConstantBuffer(RGResourceName name)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		return RGBufferConstantId(handle);
	}

	RGRenderTargetId RenderGraph::RenderTarget(RGResourceName name, GfxTextureDescriptorDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidTextureHandle(handle), "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::RenderTarget;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::RTV;
		}
		std::vector<std::pair<GfxTextureDescriptorDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::RenderTarget) return RGRenderTargetId(i, handle);
		}
		uint64 view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::RenderTarget);
		return RGRenderTargetId(view_id, handle);
	}

	RGDepthStencilId RenderGraph::DepthStencil(RGResourceName name, GfxTextureDescriptorDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidTextureHandle(handle), "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::DepthStencil;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::DSV;
		}
		std::vector<std::pair<GfxTextureDescriptorDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::DepthStencil) return RGDepthStencilId(i, handle);
		}
		uint64 view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::DepthStencil);
		return RGDepthStencilId(view_id, handle);
	}

	RGTextureReadOnlyId RenderGraph::ReadTexture(RGResourceName name, GfxTextureDescriptorDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidTextureHandle(handle), "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::ShaderResource;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::PixelSRV | GfxResourceState::ComputeSRV;
		}
		std::vector<std::pair<GfxTextureDescriptorDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadOnly) return RGTextureReadOnlyId(i, handle);
		}
		uint64 view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadOnly);
		return RGTextureReadOnlyId(view_id, handle);
	}

	RGTextureReadWriteId RenderGraph::WriteTexture(RGResourceName name, GfxTextureDescriptorDesc const& desc)
	{
		RGTextureId handle = texture_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidTextureHandle(handle), "Resource has not been declared!");
		RGTexture* rg_texture = GetRGTexture(handle);
		rg_texture->desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		if (rg_texture->desc.initial_state == GfxResourceState::Common)
		{
			rg_texture->desc.initial_state = GfxResourceState::AllUAV;
		}
		std::vector<std::pair<GfxTextureDescriptorDesc, RGDescriptorType>>& view_descs = texture_view_desc_map[handle];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadWrite) return RGTextureReadWriteId(i, handle);
		}
		uint64 view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadWrite);
		return RGTextureReadWriteId(view_id, handle);
	}

	RGBufferReadOnlyId RenderGraph::ReadBuffer(RGResourceName name, GfxBufferDescriptorDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= GfxBindFlag::ShaderResource;
		std::vector<std::pair<GfxBufferDescriptorDesc, RGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadOnly) return RGBufferReadOnlyId(i, handle);
		}
		uint64 view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadOnly);
		return RGBufferReadOnlyId(view_id, handle);
	}

	RGBufferReadWriteId RenderGraph::WriteBuffer(RGResourceName name, GfxBufferDescriptorDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		RGBuffer* rg_buffer = GetRGBuffer(handle);
		rg_buffer->desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		std::vector<std::pair<GfxBufferDescriptorDesc, RGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadWrite) return RGBufferReadWriteId(i, handle);
		}
		uint64 view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadWrite);
		return RGBufferReadWriteId(view_id, handle);
	}

	RGBufferReadWriteId RenderGraph::WriteBuffer(RGResourceName name, RGResourceName counter_name, GfxBufferDescriptorDesc const& desc)
	{
		RGBufferId handle = buffer_name_id_map[name];
		RGBufferId counter_handle = buffer_name_id_map[counter_name];
		ADRIA_ASSERT_MSG(IsValidBufferHandle(handle), "Resource has not been declared!");
		ADRIA_ASSERT_MSG(IsValidBufferHandle(counter_handle), "Resource has not been declared!");

		RGBuffer* rg_buffer = GetRGBuffer(handle);
		RGBuffer* rg_counter_buffer = GetRGBuffer(counter_handle);

		rg_buffer->desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		rg_counter_buffer->desc.bind_flags |= GfxBindFlag::UnorderedAccess;

		std::vector<std::pair<GfxBufferDescriptorDesc, RGDescriptorType>>& view_descs = buffer_view_desc_map[handle];
		for (uint64 i = 0; i < view_descs.size(); ++i)
		{
			auto const& [_desc, _type] = view_descs[i];
			if (desc == _desc && _type == RGDescriptorType::ReadWrite)
			{
				RGBufferReadWriteId rw_id(i, handle);
				if (auto it = buffer_uav_counter_map.find(rw_id); it != buffer_uav_counter_map.end())
				{
					if (it->second == counter_handle) return rw_id;
				}
			}
		}
		uint64 view_id = view_descs.size();
		view_descs.emplace_back(desc, RGDescriptorType::ReadWrite);
		RGBufferReadWriteId rw_id = RGBufferReadWriteId(view_id, handle);
		buffer_uav_counter_map.insert(std::make_pair(rw_id, counter_handle));
		return rw_id;
	}

	GfxTexture const& RenderGraph::GetCopySrcTexture(RGTextureCopySrcId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	GfxTexture& RenderGraph::GetCopyDstTexture(RGTextureCopyDstId res_id) const
	{
		return *GetTexture(RGTextureId(res_id));
	}

	GfxBuffer const& RenderGraph::GetCopySrcBuffer(RGBufferCopySrcId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer& RenderGraph::GetCopyDstBuffer(RGBufferCopyDstId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetVertexBuffer(RGBufferVertexId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetIndexBuffer(RGBufferIndexId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxBuffer const& RenderGraph::GetConstantBuffer(RGBufferConstantId res_id) const
	{
		return *GetBuffer(RGBufferId(res_id));
	}

	GfxDescriptor RenderGraph::GetRenderTarget(RGRenderTargetId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetDepthStencil(RGDepthStencilId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadOnlyTexture(RGTextureReadOnlyId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadWriteTexture(RGTextureReadWriteId res_id) const
	{
		RGTextureId tex_id = res_id.GetResourceId();
		auto const& views = texture_view_map[tex_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const
	{
		RGBufferId buf_id = res_id.GetResourceId();
		auto const& views = buffer_view_map[buf_id];
		return views[res_id.GetViewId()].first;
	}

	GfxDescriptor RenderGraph::GetReadWriteBuffer(RGBufferReadWriteId res_id) const
	{
		RGBufferId buf_id = res_id.GetResourceId();
		auto const& views = buffer_view_map[buf_id];
		return views[res_id.GetViewId()].first;
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

	void RenderGraph::DependencyLevel::Execute(GfxDevice* gfx, GfxCommandList* cmd_list)
	{
		for (auto& pass : passes)
		{
			if (pass->IsCulled()) continue;
			RenderGraphContext rg_resources(rg, *pass);
			if (pass->type == RGPassType::Graphics)
			{
				GfxRenderPassDesc render_pass_desc{};
				render_pass_desc.flags = GfxRenderPassFlagBit_None;
				render_pass_desc.rtv_attachments.reserve(pass->render_targets_info.size());
				for (auto const& render_target_info : pass->render_targets_info)
				{
					GfxColorAttachmentDesc rtv_desc{};

					RGLoadAccessOp load_access = RGLoadAccessOp::NoAccess;
					RGStoreAccessOp store_access = RGStoreAccessOp::NoAccess;
					SplitAccessOp(render_target_info.render_target_access, load_access, store_access);

					switch (load_access)
					{
					case RGLoadAccessOp::Clear:
						rtv_desc.beginning_access = GfxLoadAccessOp::Clear;
						break;
					case RGLoadAccessOp::Discard:
						rtv_desc.beginning_access = GfxLoadAccessOp::Discard;
						break;
					case RGLoadAccessOp::Preserve:
						rtv_desc.beginning_access = GfxLoadAccessOp::Preserve;
						break;
					case RGLoadAccessOp::NoAccess:
						rtv_desc.beginning_access = GfxLoadAccessOp::NoAccess;
						break;
					default:
						ADRIA_ASSERT_MSG(false, "Invalid Load Access!");
					}

					switch (store_access)
					{
					case RGStoreAccessOp::Resolve:
						rtv_desc.ending_access = GfxStoreAccessOp::Resolve;
						break;
					case RGStoreAccessOp::Discard:
						rtv_desc.ending_access = GfxStoreAccessOp::Discard;
						break;
					case RGStoreAccessOp::Preserve:
						rtv_desc.ending_access = GfxStoreAccessOp::Preserve;
						break;
					case RGStoreAccessOp::NoAccess:
						rtv_desc.ending_access = GfxStoreAccessOp::NoAccess;
						break;
					default:
						ADRIA_ASSERT_MSG(false, "Invalid Store Access!");
					}

					RGTextureId rt_texture = render_target_info.render_target_handle.GetResourceId();
					GfxTexture* texture = rg.GetTexture(rt_texture);

					GfxTextureDesc const& desc = texture->GetDesc();
					GfxClearValue const& clear_value = desc.clear_value;
					if (clear_value.active_member != GfxClearValue::GfxActiveMember::None)
					{
						ADRIA_ASSERT_MSG(clear_value.active_member == GfxClearValue::GfxActiveMember::Color, "Invalid Clear Value for Render Target");
						rtv_desc.clear_value = desc.clear_value;
						rtv_desc.clear_value.format = desc.format;
					}
					else if(rtv_desc.beginning_access == GfxLoadAccessOp::Clear)
					{
						rtv_desc.clear_value.format = desc.format;
						rtv_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
					}

					rtv_desc.cpu_handle = rg.GetRenderTarget(render_target_info.render_target_handle);
					render_pass_desc.rtv_attachments.push_back(rtv_desc);
				}

				if (pass->depth_stencil.has_value())
				{
					auto const& depth_stencil_info = pass->depth_stencil.value();
					if (depth_stencil_info.depth_read_only)
					{
						render_pass_desc.flags |= GfxRenderPassFlagBit_ReadOnlyDepth;
					}
					
					GfxDepthAttachmentDesc dsv_desc{};
					RGLoadAccessOp load_access = RGLoadAccessOp::NoAccess;
					RGStoreAccessOp store_access = RGStoreAccessOp::NoAccess;
					SplitAccessOp(depth_stencil_info.depth_access, load_access, store_access);

					switch (load_access)
					{
					case RGLoadAccessOp::Clear:
						dsv_desc.depth_beginning_access = GfxLoadAccessOp::Clear;
						break;
					case RGLoadAccessOp::Discard:
						dsv_desc.depth_beginning_access = GfxLoadAccessOp::Discard;
						break;
					case RGLoadAccessOp::Preserve:
						dsv_desc.depth_beginning_access = GfxLoadAccessOp::Preserve;
						break;
					case RGLoadAccessOp::NoAccess:
						dsv_desc.depth_beginning_access = GfxLoadAccessOp::NoAccess;
						break;
					default:
						ADRIA_ASSERT_MSG(false, "Invalid Load Access!");
					}

					switch (store_access)
					{
					case RGStoreAccessOp::Resolve:
						dsv_desc.depth_ending_access = GfxStoreAccessOp::Resolve;
						break;
					case RGStoreAccessOp::Discard:
						dsv_desc.depth_ending_access = GfxStoreAccessOp::Discard;
						break;
					case RGStoreAccessOp::Preserve:
						dsv_desc.depth_ending_access = GfxStoreAccessOp::Preserve;
						break;
					case RGStoreAccessOp::NoAccess:
						dsv_desc.depth_ending_access = GfxStoreAccessOp::NoAccess;
						break;
					default:
						ADRIA_ASSERT_MSG(false, "Invalid Store Access!");
					}

					RGTextureId ds_texture = depth_stencil_info.depth_stencil_handle.GetResourceId();
					GfxTexture* texture = rg.GetTexture(ds_texture);

					GfxTextureDesc const& desc = texture->GetDesc();
					if (desc.clear_value.active_member != GfxClearValue::GfxActiveMember::None)
					{
						ADRIA_ASSERT_MSG(desc.clear_value.active_member == GfxClearValue::GfxActiveMember::DepthStencil, "Invalid Clear Value for Depth Stencil");
						dsv_desc.clear_value = desc.clear_value;
						dsv_desc.clear_value.format = desc.format;
					}
					else if (dsv_desc.depth_beginning_access == GfxLoadAccessOp::Clear)
					{
						dsv_desc.clear_value.format = desc.format;
						dsv_desc.clear_value = GfxClearValue(0.0f, 0);
					}

					dsv_desc.cpu_handle = rg.GetDepthStencil(depth_stencil_info.depth_stencil_handle);

					//todo add stencil
					render_pass_desc.dsv_attachment = dsv_desc;
				}
				ADRIA_ASSERT_MSG((pass->viewport_width != 0 && pass->viewport_height != 0), "Viewport Width/Height is 0! The call to builder.SetViewport is probably missing...");
				render_pass_desc.width = pass->viewport_width;
				render_pass_desc.height = pass->viewport_height;
				render_pass_desc.legacy = pass->UseLegacyRenderPasses();

				PIXScopedEvent(cmd_list->GetNative(), PIX_COLOR_DEFAULT, pass->name.c_str());
				AdriaGfxProfileScope(cmd_list, pass->name.c_str());
				TracyGfxProfileScope(cmd_list->GetNative(), pass->name.c_str());
				cmd_list->SetContext(GfxCommandList::Context::Graphics);
				cmd_list->BeginRenderPass(render_pass_desc);
				pass->Execute(rg_resources,cmd_list);
				cmd_list->EndRenderPass();
			}
			else
			{
				PIXScopedEvent(cmd_list->GetNative(), PIX_COLOR_DEFAULT, pass->name.c_str());
				AdriaGfxProfileScope(cmd_list, pass->name.c_str());
				TracyGfxProfileScope(cmd_list->GetNative(), pass->name.c_str());
				cmd_list->SetContext(GfxCommandList::Context::Compute);
				pass->Execute(rg_resources, cmd_list);
			}
		}
	}

	void RenderGraph::DependencyLevel::Execute(GfxDevice* gfx, std::span<GfxCommandList*> const& cmd_lists)
	{
		ADRIA_ASSERT_MSG(false, "Not yet implemented");
	}

	void RenderGraph::Dump(char const* graph_file_name)
	{
		static struct GraphVizStyle
		{
			char const* rank_dir{ "TB" };
			struct
			{
				char const* name{ "helvetica" };
				int32       size{ 10 };
			} font;
			struct
			{
				struct
				{
					char const* executed{ "orange" };
					char const* culled{ "lightgray" };
				} pass;
				struct
				{
					char const* imported{ "lightsteelblue" };
					char const* transient{ "skyblue" };
				} resource;
				struct
				{
					char const* read{ "olivedrab3" };
					char const* write{ "orangered" };
				} edge;
			} color;
		} style;

		struct GraphViz
		{
			std::string defaults;
			std::string declarations;
			std::string dependencies;
		} graphviz;

		graphviz.defaults += std::format("graph [style=invis, rankdir=\"{}\", ordering=out, splines=spline]\n", style.rank_dir);
		graphviz.defaults += std::format("node [shape=record, fontname=\"{}\", fontsize={}, margin=\"0.2,0.03\"]\n", style.font.name, style.font.size);

		auto PairHash = [](std::pair<uint64, uint64> const& p)
		{
			return std::hash<uint64>{}(p.first) + std::hash<uint64>{}(p.second);
		};
		std::unordered_set<std::pair<uint64, uint64>, decltype(PairHash)> declared_buffers;
		std::unordered_set<std::pair<uint64, uint64>, decltype(PairHash)> declared_textures;
		auto DeclareBuffer  = [&declared_buffers,&graphviz, this](RGBuffer* buffer)
		{
			auto decl_pair = std::make_pair(buffer->id, buffer->version);
			if (!declared_buffers.contains(decl_pair))
			{
				buffer->desc.size;
				graphviz.declarations += std::format("B{}_{} ", buffer->id, buffer->version);
				std::string label = std::format("<{}<br/>dimension: Buffer<br/>size: {} bytes <br/>format: {} <br/>version: {} <br/>refs: {}<br/>{}>", 
					buffer->name, buffer->desc.size, GfxFormatToString(buffer->desc.format), buffer->version, buffer->ref_count, buffer->imported ? "Imported" : "Transient");
				graphviz.declarations += std::format("[shape=\"box\", style=\"filled\",fillcolor={}, label={}] \n", buffer->imported ? style.color.resource.imported : style.color.resource.transient, label);
				declared_buffers.insert(decl_pair);
			}
		};
		auto DeclareTexture = [&declared_textures, &graphviz, this](RGTexture* texture)
		{
			auto decl_pair = std::make_pair(texture->id, texture->version);
			if (!declared_textures.contains(decl_pair))
			{
				std::string dimensions;
				switch (texture->desc.type)
				{
				case GfxTextureType_1D:  dimensions += std::format("width = {}", texture->desc.width); break;
				case GfxTextureType_2D:  dimensions += std::format("width = {}, height = {}", texture->desc.width, texture->desc.height); break;
				case GfxTextureType_3D:  dimensions += std::format("width = {}, height = {}, depth = {}", texture->desc.width, texture->desc.height, texture->desc.depth); break;
				}

				if (texture->desc.array_size > 1)  dimensions += std::format(", array size = {}", texture->desc.array_size);
				
				graphviz.declarations += std::format("T{}_{} ", texture->id, texture->version);
				std::string label = std::format("<{} <br/>dimension: {}<br/>{}<br/>format: {} <br/>version: {} <br/>refs: {}<br/>{}>", 
					texture->name, GfxTextureTypeToString(texture->desc.type), dimensions, GfxFormatToString(texture->desc.format), texture->version, texture->ref_count, texture->imported ? "Imported" : "Transient");
				graphviz.declarations += std::format("[shape=\"box\", style=\"filled\",fillcolor={}, label={}] \n", texture->imported ? style.color.resource.imported : style.color.resource.transient, label);
				declared_textures.insert(decl_pair);
			}
		};

		for (auto const& dependency_level : dependency_levels)
		{
			for (auto const& pass : dependency_level.passes)
			{
				graphviz.declarations += std::format("P{} ", pass->id);
				std::string label = std::format("<{}<br/> type: {}<br/> refs: {}<br/> culled: {}>", pass->name, RGPassTypeToString(pass->type), pass->ref_count, pass->IsCulled() ? "Yes" : "No");
				graphviz.declarations += std::format("[shape=\"ellipse\", style=\"rounded,filled\",fillcolor={}, label={}] \n",
					                                  pass->IsCulled() ?  style.color.pass.culled : style.color.pass.executed, label);

				std::string read_dependencies = "{"; 
				std::string write_dependencies = "{";

				for (auto const& buffer_read : pass->buffer_reads)
				{
					RGBuffer* buffer = GetRGBuffer(buffer_read);
					DeclareBuffer(buffer);
					read_dependencies += std::format("B{}_{},", buffer->id, buffer->version);
				}

				for (auto const& texture_read : pass->texture_reads)
				{
					RGTexture* texture = GetRGTexture(texture_read);
					DeclareTexture(texture);
					read_dependencies += std::format("T{}_{},", texture->id, texture->version);
				}
				
				for (auto const& buffer_write : pass->buffer_writes)
				{
					RGBuffer* buffer = GetRGBuffer(buffer_write);
					if (!pass->buffer_creates.contains(buffer_write)) buffer->version++;
					DeclareBuffer(buffer);
					write_dependencies += std::format("B{}_{},", buffer->id, buffer->version);
				}

				for (auto const& texture_write : pass->texture_writes)
				{
					RGTexture* texture = GetRGTexture(texture_write);
					if (!pass->texture_creates.contains(texture_write)) texture->version++;
					DeclareTexture(texture);
					write_dependencies += std::format("T{}_{},", texture->id, texture->version);
				}

				if (read_dependencies.back() == ',') read_dependencies.pop_back(); 
				read_dependencies += "}";
				if (write_dependencies.back() == ',') write_dependencies.pop_back();
				write_dependencies += "}";

				graphviz.dependencies += std::format("{}->P{} [color=olivedrab3]\n", read_dependencies, pass->id);
				graphviz.dependencies += std::format("P{}->{} [color=orangered]\n", pass->id, write_dependencies);
			}
		}

		std::string absolute_graph_path = paths::RenderGraphDir + graph_file_name;
		std::ofstream graph_file(absolute_graph_path);
		graph_file << "digraph RenderGraph{ \n";
		graph_file << graphviz.defaults << "\n";
		graph_file << graphviz.declarations << "\n";
		graph_file << graphviz.dependencies << "\n";
		graph_file << "}";
		graph_file.close();

		std::string filename = GetFilenameWithoutExtension(graph_file_name);
		std::string cmd = std::format("Tools\\graphviz\\dot.exe -Tsvg {} > {}{}.svg", absolute_graph_path, paths::RenderGraphDir, filename);
		system(cmd.c_str());
	}

	void RenderGraph::DumpDebugData()
	{
		std::string render_graph_data = "";
		render_graph_data += "Passes: \n";
		for (uint64 i = 0; i < passes.size(); ++i)
		{
			auto& pass = passes[i];
			render_graph_data += std::format("Pass {}: {}\n", i, pass->name);
			render_graph_data += "\nTexture usage:\n";
			for (auto [tex_id, state] : pass->texture_state_map)
			{
				render_graph_data += std::format("Texture ID: {}, State: {}\n", tex_id.id, ConvertBarrierFlagsToString(state));
			}
			render_graph_data += "\nBuffer usage:\n";
			for (auto [buf_id, state] : pass->buffer_state_map)
			{
				render_graph_data += std::format("Buffer ID: {}, State: {}\n", buf_id.id, ConvertBarrierFlagsToString(state));
			}
			render_graph_data += "\n";
		}

		render_graph_data += "\nAdjacency lists: \n";
		for (uint64 i = 0; i < adjacency_lists.size(); ++i)
		{
			auto& list = adjacency_lists[i];
			render_graph_data += std::format("{}. {}'s adjacency list: ", i, passes[i]->name);
			for (auto j : list) render_graph_data += std::format(" {} ", j);
			render_graph_data += "\n";
		}

		render_graph_data += "\nTopologically sorted passes: \n";
		for (uint64 i = 0; i < topologically_sorted_passes.size(); ++i)
		{
			auto& topologically_sorted_pass = topologically_sorted_passes[i];
			render_graph_data += std::format("{}. : {}\n", i, passes[topologically_sorted_pass]->name);
		}

		render_graph_data += "\nDependency levels: \n";
		for (uint64 i = 0; i < dependency_levels.size(); ++i)
		{
			auto& level = dependency_levels[i];
			render_graph_data += std::format("Dependency level {}: \n", i);
			for(auto pass : level.passes) render_graph_data += std::format("{}\n", pass->name);
			render_graph_data += "\nTexture usage:\n";
			for (auto [tex_id, state] : level.texture_state_map)
			{
				render_graph_data += std::format("Texture ID: {}, State: {}\n", tex_id.id, ConvertBarrierFlagsToString(state));
			}
			render_graph_data += "\nBuffer usage:\n";
			for (auto [buf_id, state] : level.buffer_state_map)
			{
				render_graph_data += std::format("Buffer ID: {}, State: {}\n", buf_id.id, ConvertBarrierFlagsToString(state));
			}
			render_graph_data += "\n";
		}
		render_graph_data += "\nTextures: \n";
		for (uint64 i = 0; i < textures.size(); ++i)
		{
			auto& texture = textures[i];
			render_graph_data += std::format("Texture: id = {}, name = {}, last used by: {} \n", texture->id, texture->name, texture->last_used_by->name);
		}
		render_graph_data += "\nBuffers: \n";
		for (uint64 i = 0; i < buffers.size(); ++i)
		{
			auto& buffer = buffers[i];
			render_graph_data += std::format("Buffer: id = {}, name = {}, last used by: {} \n", buffer->id, buffer->name, buffer->last_used_by->name);
		}
		ADRIA_LOG(DEBUG, "[RenderGraph]\n%s", render_graph_data.c_str());
	}

}

 