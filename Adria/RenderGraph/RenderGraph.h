#pragma once
#include <stack>
#include <array>
#include <mutex>
#include "RenderGraphBlackboard.h"
#include "RenderGraphPass.h"
#include "RenderGraphResources.h"
#include "RenderGraphResourcePool.h"
#include "../Graphics/GraphicsDeviceDX12.h"

namespace adria
{
	class RenderGraphBuilder
	{
		friend class RenderGraph;

	public:
		RenderGraphBuilder() = delete;
		RenderGraphBuilder(RenderGraphBuilder const&) = delete;
		RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

		RGTextureRef CreateTexture(char const* name, TextureDesc const& desc);

		RGTextureRef Read(RGTextureRef handle, ERGReadAccess read_flag = ReadAccess_PixelShader);
		RGTextureRef Write(RGTextureRef handle, ERGWriteAccess write_flag = WriteAccess_Unordered);
		RGTextureRef RenderTarget(RGTextureRTVRef rtv_handle, ERGLoadStoreAccessOp load_store_op);
		RGTextureRef DepthStencil(RGTextureDSVRef dsv_handle, ERGLoadStoreAccessOp depth_load_store_op, bool readonly = false, 
			ERGLoadStoreAccessOp stencil_load_store_op = ERGLoadStoreAccessOp::NoAccess_NoAccess);

		RGTextureSRVRef CreateSRV(RGTextureRef handle, TextureViewDesc const& desc = {});
		RGTextureUAVRef CreateUAV(RGTextureRef handle, TextureViewDesc const& desc = {});
		RGTextureRTVRef CreateRTV(RGTextureRef handle, TextureViewDesc const& desc = {});
		RGTextureDSVRef CreateDSV(RGTextureRef handle, TextureViewDesc const& desc = {});

		void SetViewport(uint32 width, uint32 height);
	private:
		RenderGraphBuilder(RenderGraph&, RenderGraphPassBase&);

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;
	};
	using RGBuilder = RenderGraphBuilder;


	class RenderGraph
	{
		static constexpr bool Multithreaded = false;

		friend class RenderGraphBuilder;
		friend class RenderGraphResources;

		class DependencyLevel
		{
			friend RenderGraph;
		public:

			explicit DependencyLevel(RenderGraph& rg) : rg(rg) {}
			void AddPass(RenderGraphPassBase* pass);
			void Setup();
			void Execute(GraphicsDevice* gfx, CommandList* cmd_list);
			void Execute(GraphicsDevice* gfx, std::vector<CommandList*> const& cmd_lists);
			size_t GetSize() const;
			size_t GetNonCulledSize() const;

		private:
			RenderGraph& rg;
			std::vector<RenderGraphPassBase*>   passes;
			std::unordered_set<RGTextureRef> creates;
			std::unordered_set<RGTextureRef> reads;
			std::unordered_set<RGTextureRef> writes;
			std::unordered_set<RGTextureRef> destroys;
			std::unordered_map<RGTextureRef, ResourceState> required_states;
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
		[[maybe_unused]] PassData const& AddPass(Args&&... args)
		{
			passes.emplace_back(std::make_unique<RenderGraphPass<PassData>>(std::forward<Args>(args)...));
			RenderGraphBuilder builder(*this, *passes.back());
			passes.back()->Setup(builder);
			return (*dynamic_cast<RenderGraphPass<PassData>*>(passes.back().get())).GetPassData();
		}

		RGTextureRef CreateTexture(char const* name, TextureDesc const& desc);
		RGTextureRef ImportTexture(char const* name, Texture* texture);
		Texture* GetTexture(RGTextureRef) const;

		RGBufferRef CreateBuffer(char const* name, BufferDesc const& desc);
		Buffer* GetBuffer(RGBufferRef) const;

		void Build();
		void Execute();

		bool IsValidTextureHandle(RGTextureRef) const;
		bool IsValidBufferHandle(RGBufferRef) const;

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

	private:
		RGResourcePool& pool;
		GraphicsDevice* gfx;
		RGBlackboard blackboard;

		std::vector<std::unique_ptr<RGPassBase>> passes;
		std::vector<std::unique_ptr<RGTexture>> textures;
		std::vector<std::unique_ptr<RGBuffer>> buffers;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<RenderGraphPassBase*> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		mutable std::unordered_map<RGTextureRef, std::vector<TextureViewDesc>> view_desc_map;
		mutable std::unordered_map<RGTextureSRVRef, ResourceView> texture_srv_cache;
		mutable std::unordered_map<RGTextureUAVRef, ResourceView> texture_uav_cache;
		mutable std::unordered_map<RGTextureRTVRef, ResourceView> texture_rtv_cache;
		mutable std::unordered_map<RGTextureDSVRef, ResourceView> texture_dsv_cache;

		mutable std::mutex srv_cache_mutex;
		mutable std::mutex uav_cache_mutex;
		mutable std::mutex rtv_cache_mutex;
		mutable std::mutex dsv_cache_mutex;
		static constexpr size_t  i = sizeof(std::mutex);
	private:
		RGTexture* GetRGTexture(RGTextureRef handle) const;
		RGBuffer* GetRGBuffer(RGBufferRef handle) const;

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);

		RGTextureSRVRef CreateSRV(RGTextureRef handle, TextureViewDesc const& desc);
		RGTextureUAVRef CreateUAV(RGTextureRef handle, TextureViewDesc const& desc);
		RGTextureRTVRef CreateRTV(RGTextureRef handle, TextureViewDesc const& desc);
		RGTextureDSVRef CreateDSV(RGTextureRef handle, TextureViewDesc const& desc);

		ResourceView GetSRV(RGTextureSRVRef handle) const;
		ResourceView GetUAV(RGTextureUAVRef handle) const;
		ResourceView GetRTV(RGTextureRTVRef handle) const;
		ResourceView GetDSV(RGTextureDSVRef handle) const;

		void Execute_Singlethreaded();
		void Execute_Multithreaded();
	};

}
