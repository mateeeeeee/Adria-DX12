#pragma once
#include <stack>
#include <array>
#include <mutex>
#include "RenderGraphBlackboard.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcePool.h"
#include "../Graphics/GraphicsDeviceDX12.h"

namespace adria
{
	class RenderGraph
	{
		friend class RenderGraphBuilder;
		friend class RenderGraphResources;

		class DependencyLevel
		{
			friend RenderGraph;
		public:

			explicit DependencyLevel(RenderGraph& rg) : rg(rg) {}
			void AddPass(RenderGraphPassBase* pass);
			void Setup();
			void Execute(GraphicsDevice* gfx, RGCommandList* cmd_list);
			void Execute(GraphicsDevice* gfx, std::vector<RGCommandList*> const& cmd_lists);
			size_t GetSize() const;
			size_t GetNonCulledSize() const;

		private:
			RenderGraph& rg;
			std::vector<RenderGraphPassBase*> passes;
			tsl::robin_set<RGTextureId> texture_creates;
			tsl::robin_set<RGTextureId> texture_reads;
			tsl::robin_set<RGTextureId> texture_writes;
			tsl::robin_set<RGTextureId> texture_destroys;
			tsl::robin_map<RGTextureId, RGResourceState> texture_state_map;

			tsl::robin_set<RGBufferId> buffer_creates;
			tsl::robin_set<RGBufferId> buffer_reads;
			tsl::robin_set<RGBufferId> buffer_writes;
			tsl::robin_set<RGBufferId> buffer_destroys;
			tsl::robin_map<RGBufferId, RGResourceState> buffer_state_map;
		};

	public:

		RenderGraph(RGResourcePool& pool) : pool(pool), gfx(pool.GetDevice())
		{}
		RenderGraph(RenderGraph const&) = delete;
		RenderGraph(RenderGraph&&) = default;
		RenderGraph& operator=(RenderGraph const&) = delete;
		RenderGraph& operator=(RenderGraph&&) = default;
		~RenderGraph()
		{
			for (auto& [_, view] : texture_srv_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (auto& [_, view] : texture_uav_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (auto& [_, view] : texture_rtv_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			for (auto& [_, view] : texture_dsv_cache) gfx->FreeOfflineDescriptor(view, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		template<typename PassData, typename... Args> requires std::is_constructible_v<RenderGraphPass<PassData>, Args...>
		[[maybe_unused]] decltype(auto) AddPass(Args&&... args)
		{
			passes.emplace_back(std::make_unique<RenderGraphPass<PassData>>(std::forward<Args>(args)...));
			RenderGraphBuilder builder(*this, *passes.back());
			passes.back()->Setup(builder);
			return *dynamic_cast<RenderGraphPass<PassData>*>(passes.back().get());
		}

		void Build();
		void Execute();

		void ImportTexture(RGResourceName name, Texture* texture);
		void ImportBuffer(RGResourceName name, Buffer* buffer);

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

	private:
		RGResourcePool& pool;
		GraphicsDevice* gfx;
		RGBlackboard blackboard;

		std::vector<std::unique_ptr<RGPassBase>> passes;
		std::vector<std::unique_ptr<RGTexture>> textures;
		std::vector<std::unique_ptr<RGBuffer>> buffers;
		std::vector<DynamicAllocation> dynamic_allocations;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<size_t> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		std::unordered_map<RGResourceName, RGTextureId> texture_name_id_map;
		std::unordered_map<RGResourceName, RGBufferId>  buffer_name_id_map;
		std::unordered_map<RGResourceName, RGAllocationId>  alloc_name_id_map;

		mutable std::unordered_map<RGTextureId, std::vector<TextureViewDesc>> texture_view_desc_map;
		mutable std::unordered_map<RGTextureReadOnlyId, RGDescriptor> texture_srv_cache;
		mutable std::unordered_map<RGTextureReadWriteId, RGDescriptor> texture_uav_cache;
		mutable std::unordered_map<RGRenderTargetId, RGDescriptor> texture_rtv_cache;
		mutable std::unordered_map<RGDepthStencilId, RGDescriptor> texture_dsv_cache;

		mutable std::unordered_map<RGBufferId, std::vector<BufferViewDesc>> buffer_view_desc_map;
		mutable std::unordered_map<RGBufferReadOnlyId, RGDescriptor> buffer_srv_cache;
		mutable std::unordered_map<RGBufferReadWriteId, RGDescriptor> buffer_uav_cache;

#ifdef RG_MULTITHREADED //if views are created before and not on demand maybe mutexes are not neccessary?
		mutable std::mutex srv_cache_mutex;
		mutable std::mutex uav_cache_mutex;
		mutable std::mutex rtv_cache_mutex;
		mutable std::mutex dsv_cache_mutex;
		mutable std::mutex buffer_srv_cache_mutex;
		mutable std::mutex buffer_uav_cache_mutex;
#endif
	private:

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);

		RGTextureId DeclareTexture(RGResourceName name, TextureDesc const& desc);
		RGBufferId DeclareBuffer(RGResourceName name, BufferDesc const& desc);
		RGAllocationId DeclareAllocation(RGResourceName name, AllocDesc const& alloc);

		bool IsTextureDeclared(RGResourceName);
		bool IsBufferDeclared(RGResourceName);
		bool IsAllocationDeclared(RGResourceName);

		bool IsValidTextureHandle(RGTextureId) const;
		bool IsValidBufferHandle(RGBufferId) const;
		bool IsValidAllocHandle(RGAllocationId) const;

		RGTexture* GetRGTexture(RGTextureId) const;
		RGBuffer* GetRGBuffer(RGBufferId) const;

		RGTextureId GetTextureId(RGResourceName);
		RGBufferId GetBufferId(RGResourceName);
		RGAllocationId UseAllocation(RGResourceName);

		RGTextureCopySrcId ReadCopySrcTexture(RGResourceName);
		RGTextureCopyDstId WriteCopyDstTexture(RGResourceName);
		RGBufferCopySrcId  ReadCopySrcBuffer(RGResourceName);
		RGBufferCopyDstId  WriteCopyDstBuffer(RGResourceName);
		RGBufferIndirectArgsId  ReadIndirectArgsBuffer(RGResourceName);

		RGRenderTargetId RenderTarget(RGResourceName name, TextureViewDesc const& desc);
		RGDepthStencilId DepthStencil(RGResourceName name, TextureViewDesc const& desc);
		RGTextureReadOnlyId ReadTexture(RGResourceName name, TextureViewDesc const& desc);
		RGTextureReadWriteId WriteTexture(RGResourceName name, TextureViewDesc const& desc);
		RGBufferReadOnlyId ReadBuffer(RGResourceName name, BufferViewDesc const& desc);
		RGBufferReadWriteId WriteBuffer(RGResourceName name, BufferViewDesc const& desc);

		//rename!!!
		Texture const& GetResource(RGTextureCopySrcId) const;
		Texture const& GetResource(RGTextureCopyDstId) const;
		Buffer const& GetResource(RGBufferCopySrcId) const;
		Buffer const& GetResource(RGBufferCopyDstId) const;
		Buffer const& GetIndirectArgsBuffer(RGBufferIndirectArgsId) const;

		RGDescriptor GetDescriptor(RGRenderTargetId) const;
		RGDescriptor GetDescriptor(RGDepthStencilId) const;
		RGDescriptor GetDescriptor(RGTextureReadOnlyId) const;
		RGDescriptor GetDescriptor(RGTextureReadWriteId) const;
		RGDescriptor GetDescriptor(RGBufferReadOnlyId) const;
		RGDescriptor GetDescriptor(RGBufferReadWriteId) const;

		DynamicAllocation& GetAllocation(RGAllocationId);
		Texture* GetTexture(RGTextureId) const;
		Buffer* GetBuffer(RGBufferId) const;

		void Execute_Singlethreaded();
		void Execute_Multithreaded();
	};

}
