#pragma once
#include <stack>
#include <array>
#include "RenderGraphBlackboard.h"
#include "RenderGraphPass.h"
#include "RenderGraphResources.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Graphics/DescriptorHeap.h"
#include "../Graphics/Heap.h"
#include "../Utilities/HashUtil.h"


namespace adria
{
	class RenderGraphResourcePool
	{
		struct PooledTexture
		{
			std::unique_ptr<Texture> texture;
			uint64 last_used_frame;
		};

	public:
		explicit RenderGraphResourcePool(GraphicsDevice* device) : device(device) {}

		void Tick()
		{
			for (size_t i = 0; i < texture_pool.size();)
			{
				PooledTexture& resource = texture_pool[i].first;
				bool active = texture_pool[i].second;
				if (!active && resource.last_used_frame + 3 < frame_index)
				{
					std::swap(texture_pool[i], texture_pool.back());
					texture_pool.pop_back();
				}
				else ++i;
			}
			++frame_index;
		}

		Texture* AllocateTexture(TextureDesc const& desc)
		{
			for (auto& [pool_texture, active] : texture_pool)
			{
				if (!active && pool_texture.texture->GetDesc() == desc)
				{
					pool_texture.last_used_frame = frame_index;
					active = true;
					return pool_texture.texture.get();
				}
			}
			return texture_pool.emplace_back(std::pair{ PooledTexture{ std::make_unique<Texture>(device, desc), frame_index}, true}).first.texture.get();
		}

		void ReleaseTexture(Texture* texture)
		{
			for (auto& [pooled_texture, active] : texture_pool)
			{
				auto& texture_ptr = pooled_texture.texture;
				if (active && texture_ptr.get() == texture)
				{
					active = false;
				}
			}
		}

	private:
		GraphicsDevice* device = nullptr;
		uint64 frame_index = 0;
		std::vector<std::pair<PooledTexture, bool>> texture_pool;
	};
	using RGResourcePool = RenderGraphResourcePool;

	class RenderGraphBuilder
	{
		friend class RenderGraph;

	public:
		RenderGraphBuilder() = delete;
		RenderGraphBuilder(RenderGraphBuilder const&) = delete;
		RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

		RGTextureHandle CreateTexture(char const* name, TextureDesc const& desc);

		RGTextureHandle Read(RGTextureHandle handle, ERGReadFlag read_flag = ReadFlag_PixelShaderAccess);
		RGTextureHandle Write(RGTextureHandle handle, ERGWriteFlag write_flag = WriteFlag_UnorderedAccess);
		RGTextureHandle RenderTarget(RGTextureHandle handle, ERGLoadStoreAccessOp load_store_op);
		RGTextureHandle DepthStencil(RGTextureHandle handle, ERGLoadStoreAccessOp depth_load_store_op, bool readonly = false, 
			ERGLoadStoreAccessOp stencil_load_store_op = ERGLoadStoreAccessOp::NoAccess_NoAccess);

		RGTextureHandleSRV CreateSRV(RGTextureHandle handle, TextureViewDesc const& desc);
		RGTextureHandleUAV CreateUAV(RGTextureHandle handle, TextureViewDesc const& desc);
		RGTextureHandleRTV CreateRTV(RGTextureHandle handle, TextureViewDesc const& desc);
		RGTextureHandleDSV CreateDSV(RGTextureHandle handle, TextureViewDesc const& desc);
	private:

		RenderGraphBuilder(RenderGraph&, RenderGraphPassBase&);

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;
	};
	using RGBuilder = RenderGraphBuilder;

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
			void SetupDestroys();
			void Execute(GraphicsDevice* gfx, CommandList* cmd_list);
			size_t GetSize() const;
			size_t GetNonCulledSize() const;

		private:
			RenderGraph& rg;
			std::vector<RenderGraphPassBase*>   passes;
			std::unordered_set<RGTextureHandle> creates;
			std::unordered_set<RGTextureHandle> reads;
			std::unordered_set<RGTextureHandle> writes;
			std::unordered_set<RGTextureHandle> destroys;
		};

	public:

		RenderGraph(GraphicsDevice* gfx, RGResourcePool& pool) : gfx(gfx), pool(pool)
		{}
		RenderGraph(RenderGraph const&) = delete;
		RenderGraph(RenderGraph&&) = default;
		RenderGraph& operator=(RenderGraph const&) = delete;
		RenderGraph& operator=(RenderGraph&&) = default;
		~RenderGraph() = default;

		template<typename PassData, typename... Args> requires std::is_constructible_v<RenderGraphPass<PassData>, Args...>
		[[maybe_unused]] RenderGraphPass<PassData> const& AddPass(Args&&... args)
		{
			passes.emplace_back(std::make_unique<RenderGraphPass<PassData>>(std::forward<Args>(args)...));
			RenderGraphBuilder builder(*this, *passes.back());
			passes.back()->Setup(builder);
			return *dynamic_cast<RenderGraphPass<PassData>*>(passes.back().get());
		}

		RGTextureHandle CreateTexture(char const* name, TextureDesc const& desc);
		Texture* GetTexture(RGTextureHandle) const;

		RGBufferHandle CreateBuffer(char const* name, BufferDesc const& desc);
		Buffer* GetBuffer(RGBufferHandle) const;

		RGTextureHandle ImportTexture(char const* name, Texture* texture);
		Texture* GetImportedTexture(RGTextureHandle);

		void Build();
		void Execute();

		bool IsValidTextureHandle(RGTextureHandle) const;
		bool IsValidBufferHandle(RGBufferHandle) const;

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

	private:
		std::vector<std::unique_ptr<RGPassBase>> passes;

		std::vector<std::unique_ptr<RGTexture>> textures;
		std::vector<RGTextureNode> texture_nodes;
		std::vector<Texture*> imported_textures;
		
		mutable std::unordered_map<RGTextureHandle, std::vector<TextureViewDesc>> view_desc_map;

		std::vector<std::unique_ptr<RGBuffer>> buffers;
		std::vector<RGBufferNode> buffer_nodes;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<RenderGraphPassBase*> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		RGBlackboard blackboard;
		RGResourcePool& pool;
		GraphicsDevice* gfx;
	private:

		RGTextureHandle CreateTextureNode(RGTexture* resource);
		RGTextureNode const& GetTextureNode(RGTextureHandle handle) const;
		RGTextureNode& GetTextureNode(RGTextureHandle handle);

		RGBufferHandle CreateBufferNode(RGBuffer* resource);
		RGBufferNode const& GetBufferNode(RGBufferHandle handle) const;
		RGBufferNode& GetBufferNode(RGBufferHandle handle);

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);

		RGTextureHandleSRV CreateSRV(RGTextureHandle handle, TextureViewDesc const& desc);
		RGTextureHandleUAV CreateUAV(RGTextureHandle handle, TextureViewDesc const& desc);
		RGTextureHandleRTV CreateRTV(RGTextureHandle handle, TextureViewDesc const& desc);
		RGTextureHandleDSV CreateDSV(RGTextureHandle handle, TextureViewDesc const& desc);

		ResourceView GetSRV(RGTextureHandleSRV handle) const;
		ResourceView GetUAV(RGTextureHandleUAV handle) const;
		ResourceView GetRTV(RGTextureHandleRTV handle) const;
		ResourceView GetDSV(RGTextureHandleDSV handle) const;
	};

}
