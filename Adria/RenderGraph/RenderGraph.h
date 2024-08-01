#pragma once
#include <array>
#include "RenderGraphBlackboard.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcePool.h"
#include "Graphics/GfxDevice.h"

namespace adria
{
	class RenderGraph
	{
		friend class RenderGraphBuilder;
		friend class RenderGraphContext;

		class DependencyLevel
		{
			friend RenderGraph;
		public:

			explicit DependencyLevel(RenderGraph& rg) : rg(rg) {}
			void AddPass(RenderGraphPassBase* pass);
			void Setup();
			void Execute(GfxDevice* gfx, GfxCommandList* cmd_list);
			void Execute(GfxDevice* gfx, std::span<GfxCommandList*> const& cmd_lists);

		private:
			RenderGraph& rg;
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
		};

	public:

		RenderGraph(RGResourcePool& pool) : pool(pool), gfx(pool.GetDevice()) {}
		ADRIA_NONCOPYABLE(RenderGraph)
		ADRIA_DEFAULT_MOVABLE(RenderGraph)
		~RenderGraph();

		void Build();
		void Execute();

		template<typename PassData, typename... Args> requires std::is_constructible_v<RenderGraphPass<PassData>, Args...>
		ADRIA_MAYBE_UNUSED decltype(auto) AddPass(Args&&... args)
		{
			passes.emplace_back(std::make_unique<RenderGraphPass<PassData>>(std::forward<Args>(args)...));
			std::unique_ptr<RGPassBase>& pass = passes.back(); pass->id = passes.size() - 1;
			RenderGraphBuilder builder(*this, *pass);
			pass->Setup(builder);
			return *dynamic_cast<RenderGraphPass<PassData>*>(pass.get());
		}

		void ImportTexture(RGResourceName name, GfxTexture* texture);
		void ImportBuffer(RGResourceName name, GfxBuffer* buffer);

		void ExportTexture(RGResourceName name, GfxTexture* texture);
		void ExportBuffer(RGResourceName name, GfxBuffer* buffer);

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

		void Dump(char const* graph_file_name);
		void DumpDebugData();

	private:
		RGResourcePool& pool;
		GfxDevice* gfx;
		RGBlackboard blackboard;

		std::vector<std::unique_ptr<RGPassBase>> passes;
		std::vector<std::unique_ptr<RGTexture>> textures;
		std::vector<std::unique_ptr<RGBuffer>> buffers;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<uint64> topologically_sorted_passes;
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
		void DepthFirstSearch(uint64 i, std::vector<bool>& visited, std::vector<uint64>& sort);
		
		RGTextureId DeclareTexture(RGResourceName name, RGTextureDesc const& desc);
		RGBufferId DeclareBuffer(RGResourceName name, RGBufferDesc const& desc);

		bool IsTextureDeclared(RGResourceName);
		bool IsBufferDeclared(RGResourceName);

		bool IsValidTextureHandle(RGTextureId) const;
		bool IsValidBufferHandle(RGBufferId) const;

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
