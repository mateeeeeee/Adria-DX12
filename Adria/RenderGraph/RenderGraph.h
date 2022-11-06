#pragma once
#include <stack>
#include <array>
#include <mutex>
#include "RenderGraphBlackboard.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcePool.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Utilities/HashSet.h"
#include "../Utilities/HashMap.h"

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
			void Execute(GraphicsDevice* gfx, CommandList* cmd_list);
			void Execute(GraphicsDevice* gfx, std::vector<CommandList*> const& cmd_lists);
			size_t GetSize() const;
			size_t GetNonCulledSize() const;

		private:
			RenderGraph& rg;
			std::vector<RenderGraphPassBase*> passes;
			HashSet<RGTextureId> texture_creates;
			HashSet<RGTextureId> texture_reads;
			HashSet<RGTextureId> texture_writes;
			HashSet<RGTextureId> texture_destroys;
			HashMap<RGTextureId, EResourceState> texture_state_map;

			HashSet<RGBufferId> buffer_creates;
			HashSet<RGBufferId> buffer_reads;
			HashSet<RGBufferId> buffer_writes;
			HashSet<RGBufferId> buffer_destroys;
			HashMap<RGBufferId, EResourceState> buffer_state_map;
		};

	public:

		RenderGraph(RGResourcePool& pool) : pool(pool), gfx(pool.GetDevice())
		{}
		RenderGraph(RenderGraph const&) = delete;
		RenderGraph(RenderGraph&&) = default;
		RenderGraph& operator=(RenderGraph const&) = delete;
		RenderGraph& operator=(RenderGraph&&) = default;
		~RenderGraph();

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

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<size_t> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		HashMap<RGResourceName, RGTextureId> texture_name_id_map;
		HashMap<RGResourceName, RGBufferId>  buffer_name_id_map;
		HashMap<RGBufferReadWriteId, RGBufferId> buffer_uav_counter_map;

		mutable HashMap<RGTextureId, std::vector<std::pair<TextureSubresourceDesc, ERGDescriptorType>>> texture_view_desc_map;
		mutable HashMap<RGTextureId, std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, ERGDescriptorType>>> texture_view_map;

		mutable HashMap<RGBufferId, std::vector<std::pair<BufferSubresourceDesc, ERGDescriptorType>>> buffer_view_desc_map;
		mutable HashMap<RGBufferId, std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, ERGDescriptorType>>> buffer_view_map;

	private:

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);

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
		void AddBufferBindFlags(RGResourceName name, EBindFlag flags);
		void AddTextureBindFlags(RGResourceName name, EBindFlag flags);

		RGTextureCopySrcId ReadCopySrcTexture(RGResourceName);
		RGTextureCopyDstId WriteCopyDstTexture(RGResourceName);
		RGBufferCopySrcId  ReadCopySrcBuffer(RGResourceName);
		RGBufferCopyDstId  WriteCopyDstBuffer(RGResourceName);
		RGBufferIndirectArgsId  ReadIndirectArgsBuffer(RGResourceName);
		RGBufferVertexId  ReadVertexBuffer(RGResourceName);
		RGBufferIndexId  ReadIndexBuffer(RGResourceName);
		RGBufferConstantId  ReadConstantBuffer(RGResourceName);

		RGRenderTargetId RenderTarget(RGResourceName name, TextureSubresourceDesc const& desc);
		RGDepthStencilId DepthStencil(RGResourceName name, TextureSubresourceDesc const& desc);
		RGTextureReadOnlyId ReadTexture(RGResourceName name, TextureSubresourceDesc const& desc);
		RGTextureReadWriteId WriteTexture(RGResourceName name, TextureSubresourceDesc const& desc);
		RGBufferReadOnlyId ReadBuffer(RGResourceName name, BufferSubresourceDesc const& desc);
		RGBufferReadWriteId WriteBuffer(RGResourceName name, BufferSubresourceDesc const& desc);
		RGBufferReadWriteId WriteBuffer(RGResourceName name, RGResourceName counter_name, BufferSubresourceDesc const& desc);

		Texture const& GetCopySrcTexture(RGTextureCopySrcId) const;
		Texture const& GetCopyDstTexture(RGTextureCopyDstId) const;
		Buffer const& GetCopySrcBuffer(RGBufferCopySrcId) const;
		Buffer const& GetCopyDstBuffer(RGBufferCopyDstId) const;
		Buffer const& GetIndirectArgsBuffer(RGBufferIndirectArgsId) const;
		Buffer const& GetVertexBuffer(RGBufferVertexId) const;
		Buffer const& GetIndexBuffer(RGBufferIndexId) const;
		Buffer const& GetConstantBuffer(RGBufferConstantId) const;

		DescriptorCPU GetRenderTarget(RGRenderTargetId) const;
		DescriptorCPU GetDepthStencil(RGDepthStencilId) const;
		DescriptorCPU GetReadOnlyTexture(RGTextureReadOnlyId) const;
		DescriptorCPU GetReadWriteTexture(RGTextureReadWriteId) const;
		DescriptorCPU GetReadOnlyBuffer(RGBufferReadOnlyId) const;
		DescriptorCPU GetReadWriteBuffer(RGBufferReadWriteId) const;

		Texture* GetTexture(RGTextureId) const;
		Buffer* GetBuffer(RGBufferId) const;

		void CreateTextureViews(RGTextureId);
		void CreateBufferViews(RGBufferId);
		void Execute_Singlethreaded();
		void Execute_Multithreaded();
	};

}
