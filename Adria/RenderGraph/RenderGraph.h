#pragma once
#include "RenderGraphBlackboard.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcePool.h"
#include "RenderGraphEvent.h"
#include "RenderGraphAllocator.h"
#include "Graphics/GfxDevice.h"

namespace adria
{
	class RenderGraph
	{
		friend class RenderGraphBuilder;
		friend class RenderGraphContext;

		struct RenderGraphExecutionContext
		{
			GfxDevice* gfx;
			GfxCommandList* graphics_cmd_list;
			GfxCommandList* compute_cmd_list;
			GfxFence* graphics_fence;
			GfxFence* compute_fence;
			Uint64 graphics_fence_value;
			Uint64 compute_fence_value;
		};

		class DependencyLevel
		{
			friend RenderGraph;
		public:

			DependencyLevel(RenderGraph& rg, Uint32 level_index) : rg(rg), level_index(level_index) {}
			void AddPass(RenderGraphPassBase* pass);
			void Setup();
			void Execute(RenderGraphExecutionContext const& exec_ctx);

		private:
			RenderGraph& rg;
			Uint32 level_index;
			std::vector<RenderGraphPassBase*> passes;
			std::unordered_set<RGTextureId> texture_creates;
			std::unordered_set<RGTextureId> texture_reads;
			std::unordered_set<RGTextureId> texture_writes;
			std::unordered_set<RGTextureId> texture_destroys;
			std::unordered_map<RGTextureId, GfxResourceState> texture_state_map;

			std::unordered_set<RGBufferId> buffer_creates;
			std::unordered_set<RGBufferId> buffer_reads;
			std::unordered_set<RGBufferId> buffer_writes;
			std::unordered_set<RGBufferId> buffer_destroys;
			std::unordered_map<RGBufferId, GfxResourceState> buffer_state_map;

		private:
			void PreExecute(GfxCommandList*);
			void PostExecute(GfxCommandList*);
		};

	public:
		RenderGraph(RGResourcePool& pool) : pool(pool), allocator(128 * 1024), gfx(pool.GetDevice()) {}
		ADRIA_NONCOPYABLE(RenderGraph)
		ADRIA_DEFAULT_MOVABLE(RenderGraph)
		~RenderGraph();

		void Compile();
		void Execute();

		template<typename PassData, typename... Args> requires std::is_constructible_v<RenderGraphPass<PassData>, Args...>
		ADRIA_MAYBE_UNUSED decltype(auto) AddPass(Args&&... args)
		{
			passes.emplace_back(allocator.AllocateObject<RenderGraphPass<PassData>>(std::forward<Args>(args)...));
			RGPassBase*& pass = passes.back(); pass->id = passes.size() - 1;
			RenderGraphBuilder builder(*this, *pass);
			pass->Setup(builder);
			for (Uint32 event_idx : pending_event_indices) pass->events_to_start.push_back(event_idx);
			pending_event_indices.clear();
			return *dynamic_cast<RenderGraphPass<PassData>*>(pass);
		}

		void ImportTexture(RGResourceName name, GfxTexture* texture);
		void ImportBuffer(RGResourceName name, GfxBuffer* buffer);

		void ExportTexture(RGResourceName name, GfxTexture* texture);
		void ExportBuffer(RGResourceName name, GfxBuffer* buffer);

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

		void PushEvent(Char const* name);
		void PopEvent();

		void Dump(Char const* graph_file_name);
		void DumpDebugData();

	private:
		RGResourcePool& pool;
		GfxDevice* gfx;
		RGAllocator allocator;
		RGBlackboard blackboard;

		std::vector<RGPassBase*> passes;
		std::vector<std::unique_ptr<RGTexture>> textures;
		std::vector<std::unique_ptr<RGBuffer>> buffers;

		std::vector<RGEvent> events;
		std::vector<Uint32>  pending_event_indices;

		std::vector<std::vector<Uint64>> adjacency_lists;
		std::vector<Uint64> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		std::unordered_map<RGResourceName, RGTextureId> texture_name_id_map;
		std::unordered_map<RGResourceName, RGBufferId>  buffer_name_id_map;
		std::unordered_map<RGBufferReadWriteId, RGBufferId> buffer_uav_counter_map;

		mutable std::unordered_map<RGTextureId, std::vector<std::pair<GfxTextureDescriptorDesc, RGDescriptorType>>> texture_view_desc_map;
		mutable std::unordered_map<RGTextureId, std::vector<std::pair<GfxDescriptor, RGDescriptorType>>> texture_view_map;

		mutable std::unordered_map<RGBufferId, std::vector<std::pair<GfxBufferDescriptorDesc, RGDescriptorType>>> buffer_view_desc_map;
		mutable std::unordered_map<RGBufferId, std::vector<std::pair<GfxDescriptor, RGDescriptorType>>> buffer_view_map;

	private:

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(Uint64 i, std::vector<Bool>& visited, std::vector<Uint64>& sort);
		void ResolveAsync();
		void ResolveEvents();
		Uint32 AddEvent(Char const* name)
		{
			events.emplace_back(name);
			return static_cast<Uint32>(events.size() - 1);
		}
		
		RGTextureId DeclareTexture(RGResourceName name, RGTextureDesc const& desc);
		RGBufferId DeclareBuffer(RGResourceName name, RGBufferDesc const& desc);

		Bool IsTextureDeclared(RGResourceName);
		Bool IsBufferDeclared(RGResourceName);

		Bool IsValidTextureHandle(RGTextureId) const;
		Bool IsValidBufferHandle(RGBufferId) const;

		RGTexture* GetRGTexture(RGTextureId) const;
		RGBuffer* GetRGBuffer(RGBufferId) const;

		RGTextureId GetTextureId(RGResourceName);
		RGBufferId GetBufferId(RGResourceName);

		RGTextureDesc GetTextureDesc(RGResourceName);
		RGBufferDesc  GetBufferDesc(RGResourceName);

		void AddBufferBindFlags(RGResourceName name, GfxBindFlag flags);
		void AddTextureBindFlags(RGResourceName name, GfxBindFlag flags);

		RGTextureCopySrcId ReadCopySrcTexture(RGResourceName);
		RGTextureCopyDstId WriteCopyDstTexture(RGResourceName);
		RGBufferCopySrcId  ReadCopySrcBuffer(RGResourceName);
		RGBufferCopyDstId  WriteCopyDstBuffer(RGResourceName);
		RGBufferIndirectArgsId  ReadIndirectArgsBuffer(RGResourceName);
		RGBufferVertexId  ReadVertexBuffer(RGResourceName);
		RGBufferIndexId  ReadIndexBuffer(RGResourceName);
		RGBufferConstantId  ReadConstantBuffer(RGResourceName);

		RGRenderTargetId RenderTarget(RGResourceName name, GfxTextureDescriptorDesc const& desc);
		RGDepthStencilId DepthStencil(RGResourceName name, GfxTextureDescriptorDesc const& desc);
		RGTextureReadOnlyId ReadTexture(RGResourceName name, GfxTextureDescriptorDesc const& desc);
		RGTextureReadWriteId WriteTexture(RGResourceName name, GfxTextureDescriptorDesc const& desc);
		RGBufferReadOnlyId ReadBuffer(RGResourceName name, GfxBufferDescriptorDesc const& desc);
		RGBufferReadWriteId WriteBuffer(RGResourceName name, GfxBufferDescriptorDesc const& desc);
		RGBufferReadWriteId WriteBuffer(RGResourceName name, RGResourceName counter_name, GfxBufferDescriptorDesc const& desc);

		GfxTexture const& GetCopySrcTexture(RGTextureCopySrcId) const;
		GfxTexture&		  GetCopyDstTexture(RGTextureCopyDstId) const;
		GfxBuffer const&  GetCopySrcBuffer(RGBufferCopySrcId) const;
		GfxBuffer&		  GetCopyDstBuffer(RGBufferCopyDstId) const;
		GfxBuffer const& GetIndirectArgsBuffer(RGBufferIndirectArgsId) const;
		GfxBuffer const& GetVertexBuffer(RGBufferVertexId) const;
		GfxBuffer const& GetIndexBuffer(RGBufferIndexId) const;
		GfxBuffer const& GetConstantBuffer(RGBufferConstantId) const;

		GfxDescriptor GetRenderTarget(RGRenderTargetId) const;
		GfxDescriptor GetDepthStencil(RGDepthStencilId) const;
		GfxDescriptor GetReadOnlyTexture(RGTextureReadOnlyId) const;
		GfxDescriptor GetReadWriteTexture(RGTextureReadWriteId) const;
		GfxDescriptor GetReadOnlyBuffer(RGBufferReadOnlyId) const;
		GfxDescriptor GetReadWriteBuffer(RGBufferReadWriteId) const;

		GfxTexture* GetTexture(RGTextureId) const;
		GfxBuffer* GetBuffer(RGBufferId) const;

		void CreateTextureViews(RGTextureId);
		void CreateBufferViews(RGBufferId);
		void Execute_Singlethreaded();
		void Execute_Multithreaded();

		void AddExportBufferCopyPass(RGResourceName export_buffer, GfxBuffer* buffer);
		void AddExportTextureCopyPass(RGResourceName export_texture, GfxTexture* texture);
	};
}
