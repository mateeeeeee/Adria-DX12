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

	enum class EImportedId : uint32
	{
		Backbuffer,
		Count
	};

	enum class EImportedViewId : uint32
	{
		Backbuffer_RTV,
		Count
	};

	static char const* ImportedIdNames[] =
	{
		"Backbuffer",
		"Count"
	};

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

		RGResourceHandle CreateTexture(char const* name, TextureDesc const& desc);
		RGResourceHandle ReadResource(RGResourceHandle const& handle);
		RGResourceHandle WriteResource(RGResourceHandle& handle);

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

			void Execute(GraphicsDevice* gfx, std::vector<ID3D12GraphicsCommandList4*> const& cmd_lists);

			void Execute(GraphicsDevice* gfx, ID3D12GraphicsCommandList4* cmd_list);

			size_t GetSize() const;

			size_t GetNonCulledSize() const;

		private:
			RenderGraph& rg;
			std::vector<RenderGraphPassBase*>    passes;
			std::unordered_set<RGResourceHandle> creates;
			std::unordered_set<RGResourceHandle> reads;
			std::unordered_set<RGResourceHandle> writes;
			std::unordered_set<RGResourceHandle> destroys;
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

		RGResourceHandle CreateTexture(char const* name, TextureDesc const& desc);
		RGTexture* GetTexture(RGResourceHandle);

		bool IsValidHandle(RGResourceHandle) const;

		RGResourceHandle ImportResource(EImportedId id, ID3D12Resource* texture);
		void ImportResourceView(EImportedViewId id, RGResourceView view);
		ID3D12Resource* GetImportedResource(EImportedId id) const;
		RGResourceView GetImportedView(EImportedViewId) const;

		void Build();
		void Execute();

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

	private:
		std::vector<std::unique_ptr<RGPassBase>> passes;
		std::vector<std::unique_ptr<RGTexture>> textures;
		std::vector<RGTextureNode> texture_nodes;

		std::vector<ID3D12Resource*> imported_resources;
		std::vector<RGResourceView> imported_views;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<RenderGraphPassBase*> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		RGBlackboard blackboard;
		RGResourcePool& pool;
		GraphicsDevice* gfx;
		//std::unique_ptr<Heap> resource_heap; need better allocator then linear if placed resources are used
	private:

		RGResourceHandle CreateResourceNode(RGTexture* resource);
		RGTextureNode& GetResourceNode(RGResourceHandle handle);

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);

		RGResourceView CreateShaderResourceView(RGResourceHandle handle, TextureViewDesc  const& desc);
		RGResourceView CreateRenderTargetView(RGResourceHandle handle, TextureViewDesc    const& desc);
		RGResourceView CreateUnorderedAccessView(RGResourceHandle handle, TextureViewDesc const& desc);
		RGResourceView CreateDepthStencilView(RGResourceHandle handle, TextureViewDesc    const& desc);
	};

}
