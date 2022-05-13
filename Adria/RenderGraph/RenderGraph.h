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

	static char const* ImportedIdNames[] =
	{
		"Backbuffer",
		"Count"
	};

	struct ResourceViews
	{
		D3D12_CPU_DESCRIPTOR_HANDLE srv = { NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE uav = { NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = { NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = { NULL };
	};

	class RenderGraphResourcePool
	{
		struct DescHasher
		{
			size_t operator()(D3D12_RESOURCE_DESC const& desc) const
			{
				using std::size_t;
				using std::hash;
				using std::string;

				size_t seed = 0;
				HashCombine(seed, desc.Width);
				HashCombine(seed, desc.Height);
				HashCombine(seed, desc.DepthOrArraySize);
				HashCombine(seed, desc.MipLevels);
				HashCombine(seed, desc.Alignment);
				HashCombine(seed, (int)desc.Dimension);
				HashCombine(seed, (int)desc.Format);
				HashCombine(seed, (int)desc.Layout);
				//HashCombine(seed, (int)desc.Flags);
				HashCombine(seed, desc.SampleDesc.Count);
				HashCombine(seed, desc.SampleDesc.Quality);
			}
		};

		struct PooledResource
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			uint64 last_used_frame;
		};

	public:
		explicit RenderGraphResourcePool(ID3D12Device* device) : device(device) {}

		void Tick()
		{
			for (size_t i = 0; i < resource_pool.size();)
			{
				PooledResource& resource = resource_pool[i].first;
				bool active = resource_pool[i].second;
				if (!active && resource.last_used_frame + 3 < frame_index)
				{
					std::swap(resource_pool[i], resource_pool.back());
					resource_pool.pop_back();
				}
				else ++i;
			}
			++frame_index;
		}

		ID3D12Resource* AllocateResource(D3D12_RESOURCE_DESC const& desc)
		{
			for (auto& [pooled_resource, active] : resource_pool)
			{
				auto& resource_ptr = pooled_resource.resource;
				if (!active && resource_ptr->GetDesc() == desc)
				{
					pooled_resource.last_used_frame = frame_index;
					return resource_ptr.Get();
				}
			}

			D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
			D3D12_CLEAR_VALUE* p_clear_value = nullptr;
			D3D12_CLEAR_VALUE clear_value{};
			clear_value.Format = desc.Format;

			if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
			{
				initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
				clear_value.Color[0] = 0.0f;
				clear_value.Color[1] = 0.0f;
				clear_value.Color[2] = 0.0f;
				clear_value.Color[3] = 0.0f;
				p_clear_value = &clear_value;
			}

			if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
			{
				initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				clear_value.DepthStencil.Depth = 1.0f;
				clear_value.DepthStencil.Stencil = 0;
				p_clear_value = &clear_value;
			}

			ID3D12Resource* resource = nullptr;
			D3D12_HEAP_PROPERTIES properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			BREAK_IF_FAILED(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc, initial_state, p_clear_value, IID_PPV_ARGS(&resource)));
			return resource_pool.emplace_back(std::pair{ PooledResource{ resource, frame_index }, true }).first.resource.Get();
		}

		void ReleaseResource(ID3D12Resource* resource)
		{
			for (auto& [pooled_resource, active] : resource_pool)
			{
				auto& resource_ptr = pooled_resource.resource;
				if (active && resource_ptr.Get() == resource)
				{
					active = false;
				}
			}
		}

	private:
		ID3D12Device* device = nullptr;
		uint64 frame_index = 0;
		std::vector<std::pair<PooledResource, bool>> resource_pool;
	};
	using RGResourcePool = RenderGraphResourcePool;

	class RenderGraphBuilder
	{
		friend class RenderGraph;

	public:
		RenderGraphBuilder() = delete;
		RenderGraphBuilder(RenderGraphBuilder const&) = delete;
		RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

		RGResourceHandle Create(char const* name, D3D12_RESOURCE_DESC const& desc);
		RGResourceHandle Read(RGResourceHandle const& handle);
		RGResourceHandle Write(RGResourceHandle& handle);

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
		{
			auto device = gfx->GetDevice();
			rtv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
			srv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 50));
			dsv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
			uav_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
		}
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

		RGResourceHandle CreateResource(char const* name, D3D12_RESOURCE_DESC const& desc);
		RGResourceHandle ImportResource(EImportedId id, ID3D12Resource* resource, ResourceViews const& views = {});

		bool IsValidHandle(RGResourceHandle) const;
		RGResource* GetResource(RGResourceHandle);
		RGResource* GetImportedResource(EImportedId);

		void Build();
		void Execute();

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

	private:
		std::vector<std::unique_ptr<RGPassBase>> passes;
		std::vector<std::unique_ptr<RGResource>> resources;
		std::vector<RGResourceNode> resource_nodes;

		std::unordered_map<EImportedId, RGResource*> imported_resources;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<RenderGraphPassBase*> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		RGBlackboard blackboard;
		RGResourcePool& pool;
		GraphicsDevice* gfx;

		std::unique_ptr<DescriptorHeap> rtv_heap;
		std::unique_ptr<DescriptorHeap> srv_heap;
		std::unique_ptr<DescriptorHeap> dsv_heap;
		std::unique_ptr<DescriptorHeap> uav_heap;
		//std::unique_ptr<Heap> resource_heap; need better allocator then linear if placed resources are used
	private:

		RGResourceHandle CreateResourceNode(RGResource* resource);
		RGResourceNode& GetResourceNode(RGResourceHandle handle);

		void CreateDescriptorHeapsAndViews();

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);
	};

}
