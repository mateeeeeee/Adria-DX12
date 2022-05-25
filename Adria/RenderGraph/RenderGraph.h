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

		class ResourceViewPool
		{
			struct DescHasher
			{
				size_t operator()(D3D12_SHADER_RESOURCE_VIEW_DESC const& desc) const
				{
					size_t seed = 0;
					HashCombine(seed, (int)desc.Format);
					HashCombine(seed, desc.Shader4ComponentMapping);
					HashCombine(seed, (int)desc.ViewDimension);
					switch (desc.ViewDimension)
					{
					case D3D12_SRV_DIMENSION_TEXTURE2D:
						HashCombine(seed, desc.Texture2D.MipLevels);
						HashCombine(seed, desc.Texture2D.MostDetailedMip);
						HashCombine(seed, desc.Texture2D.PlaneSlice);
						HashCombine(seed, desc.Texture2D.ResourceMinLODClamp);
						break;
					case D3D12_SRV_DIMENSION_TEXTURECUBE:
						HashCombine(seed, desc.TextureCube.MipLevels);
						HashCombine(seed, desc.TextureCube.MostDetailedMip);
						HashCombine(seed, desc.TextureCube.ResourceMinLODClamp);
						break;
					case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
						HashCombine(seed, desc.RaytracingAccelerationStructure.Location);
						break;
					default:
						ADRIA_ASSERT(false);
					}

					return seed;
				}
				size_t operator()(D3D12_RENDER_TARGET_VIEW_DESC const& desc) const
				{
					size_t seed = 0;
					HashCombine(seed, (int)desc.Format);
					HashCombine(seed, (int)desc.ViewDimension);
					switch (desc.ViewDimension)
					{
					case D3D12_RTV_DIMENSION_TEXTURE2D:
						HashCombine(seed, desc.Texture2D.MipSlice);
						HashCombine(seed, desc.Texture2D.PlaneSlice);
						break;
					case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
						HashCombine(seed, desc.Texture2DArray.MipSlice);
						HashCombine(seed, desc.Texture2DArray.PlaneSlice);
						HashCombine(seed, desc.Texture2DArray.FirstArraySlice);
						HashCombine(seed, desc.Texture2DArray.ArraySize);
						break;
					default:
						ADRIA_ASSERT(false);
					}
					return seed;
				}
				size_t operator()(D3D12_UNORDERED_ACCESS_VIEW_DESC const& desc) const
				{
					size_t seed = 0;
					HashCombine(seed, (int)desc.Format);
					HashCombine(seed, (int)desc.ViewDimension);
					switch (desc.ViewDimension)
					{
					case D3D12_UAV_DIMENSION_TEXTURE2D:
						HashCombine(seed, desc.Texture2D.MipSlice);
						HashCombine(seed, desc.Texture2D.PlaneSlice);
						break;
					default:
						ADRIA_ASSERT(false);
					}
					return seed;
				}
				size_t operator()(D3D12_DEPTH_STENCIL_VIEW_DESC const& desc) const
				{
					size_t seed = 0;
					HashCombine(seed, (int)desc.Format);
					HashCombine(seed, (int)desc.Flags);
					HashCombine(seed, (int)desc.ViewDimension);
					switch (desc.ViewDimension)
					{
					case D3D12_UAV_DIMENSION_TEXTURE2D:
						HashCombine(seed, desc.Texture2D.MipSlice);
						break;
					case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
						HashCombine(seed, desc.Texture2DArray.MipSlice);
						HashCombine(seed, desc.Texture2DArray.FirstArraySlice);
						HashCombine(seed, desc.Texture2DArray.ArraySize);
						break;
					default:
						ADRIA_ASSERT(false);
					}
					return seed;
				}
			};

		public:
			ResourceViewPool(RenderGraph& parent_graph) : parent_graph(parent_graph), device(parent_graph.gfx->GetDevice())
			{
				rtv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
				srv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 50));
				dsv_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
				uav_heap.reset(new DescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 25));
			}

			RGResourceView CreateShaderResourceView(RGResourceHandle handle, D3D12_SHADER_RESOURCE_VIEW_DESC const& desc) 
			{
				RGTexture* resource = parent_graph.GetResource(handle);
				ID3D12Resource* d3d12_resource = resource->resource;
				if (auto it = srv_cache.find(handle); it != srv_cache.end())
				{
					auto& srv_desc_map = it->second;
					if (auto it2 = srv_desc_map.find(desc); it2 != srv_desc_map.end())
					{
						return it2->second;
					}
					else
					{
						RGResourceView srv_handle = srv_heap->GetHandle(srv_heap_index++);
						device->CreateShaderResourceView(d3d12_resource, &desc, srv_handle);
						srv_desc_map.insert(std::pair(desc, srv_handle));
						return srv_handle;
					}
				}
				else
				{
					RGResourceView srv_handle = srv_heap->GetHandle(srv_heap_index++);
					device->CreateShaderResourceView(d3d12_resource, &desc, srv_handle);
					srv_cache[handle].insert(std::pair(desc, srv_handle));
					return srv_handle;
				}
			}

			RGResourceView CreateRenderTargetView(RGResourceHandle handle, D3D12_RENDER_TARGET_VIEW_DESC const& desc) 
			{
				RGTexture* resource = parent_graph.GetResource(handle);
				ID3D12Resource* d3d12_resource = resource->resource;
				if (auto it = rtv_cache.find(handle); it != rtv_cache.end())
				{
					auto& rtv_desc_map = it->second;
					if (auto it2 = rtv_desc_map.find(desc); it2 != rtv_desc_map.end())
					{
						return it2->second;
					}
					else
					{
						RGResourceView rtv_handle = rtv_heap->GetHandle(rtv_heap_index++);
						device->CreateRenderTargetView(d3d12_resource, &desc, rtv_handle);
						rtv_desc_map.insert(std::make_pair(desc, rtv_handle));
						return rtv_handle;
					}
				}
				else
				{
					RGResourceView rtv_handle = rtv_heap->GetHandle(rtv_heap_index++);
					device->CreateRenderTargetView(d3d12_resource, &desc, rtv_handle);
					rtv_cache[handle].insert(std::make_pair(desc, rtv_handle));
					return rtv_handle;
				}
			}

			RGResourceView CreateUnorderedAccessView(RGResourceHandle handle, D3D12_UNORDERED_ACCESS_VIEW_DESC const& desc) 
			{
				RGTexture* resource = parent_graph.GetResource(handle);
				ID3D12Resource* d3d12_resource = resource->resource;
				if (auto it = uav_cache.find(handle); it != uav_cache.end())
				{
					auto& uav_desc_map = it->second;
					if (auto it2 = uav_desc_map.find(desc); it2 != uav_desc_map.end())
					{
						return it2->second;
					}
					else
					{
						RGResourceView uav_handle = uav_heap->GetHandle(uav_heap_index++);
						device->CreateUnorderedAccessView(d3d12_resource, nullptr, &desc, uav_handle);
						uav_desc_map.insert(std::make_pair(desc, uav_handle));
						return uav_handle;
					}
				}
				else
				{
					RGResourceView uav_handle = uav_heap->GetHandle(uav_heap_index++);
					device->CreateUnorderedAccessView(d3d12_resource, nullptr, &desc, uav_handle);
					uav_cache[handle].insert(std::make_pair(desc, uav_handle));
					return uav_handle;
				}

			}

			RGResourceView CreateDepthStencilView(RGResourceHandle handle, D3D12_DEPTH_STENCIL_VIEW_DESC const& desc) 
			{
				RGTexture* resource = parent_graph.GetResource(handle);
				ID3D12Resource* d3d12_resource = resource->resource;
				if (auto it = dsv_cache.find(handle); it != dsv_cache.end())
				{
					auto& dsv_desc_map = it->second;
					if (auto it2 = dsv_desc_map.find(desc); it2 != dsv_desc_map.end())
					{
						return it2->second;
					}
					else
					{
						RGResourceView dsv_handle = dsv_heap->GetHandle(dsv_heap_index++);
						device->CreateDepthStencilView(d3d12_resource, &desc, dsv_handle);
						dsv_desc_map.insert(std::make_pair(desc, dsv_handle));
						return dsv_handle;
					}
				}
				else
				{
					RGResourceView dsv_handle = dsv_heap->GetHandle(dsv_heap_index++);
					device->CreateDepthStencilView(d3d12_resource, &desc, dsv_handle);
					dsv_cache[handle].insert(std::make_pair(desc, dsv_handle));
					return dsv_handle;
				}
			}

		private:
			RenderGraph& parent_graph;
			ID3D12Device* device;
			std::unique_ptr<DescriptorHeap> rtv_heap;
			std::unique_ptr<DescriptorHeap> srv_heap;
			std::unique_ptr<DescriptorHeap> dsv_heap;
			std::unique_ptr<DescriptorHeap> uav_heap;
			std::unordered_map<RGResourceHandle, std::unordered_map<D3D12_SHADER_RESOURCE_VIEW_DESC, RGResourceView, DescHasher>>  srv_cache;
			std::unordered_map<RGResourceHandle, std::unordered_map<D3D12_RENDER_TARGET_VIEW_DESC, RGResourceView, DescHasher>>    rtv_cache;
			std::unordered_map<RGResourceHandle, std::unordered_map<D3D12_UNORDERED_ACCESS_VIEW_DESC, RGResourceView, DescHasher>> uav_cache;
			std::unordered_map<RGResourceHandle, std::unordered_map<D3D12_DEPTH_STENCIL_VIEW_DESC, RGResourceView, DescHasher>>	   dsv_cache;
			uint32 srv_heap_index = 0;
			uint32 rtv_heap_index = 0;
			uint32 uav_heap_index = 0;
			uint32 dsv_heap_index = 0;
		};
	public:

		RenderGraph(GraphicsDevice* gfx, RGResourcePool& pool) : gfx(gfx), pool(pool), view_pool(*this)
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

		RGResourceHandle CreateResource(char const* name, D3D12_RESOURCE_DESC const& desc);
		RGResourceHandle ImportResource(EImportedId id, ID3D12Resource* resource);
		void ImportResourceView(EImportedViewId id, RGResourceView view);

		bool IsValidHandle(RGResourceHandle) const;

		RGTexture* GetResource(RGResourceHandle);

		RGTexture* GetImportedResource(EImportedId) const;
		RGResourceView GetImportedView(EImportedViewId) const;

		void Build();
		void Execute();

		RGBlackboard const& GetBlackboard() const { return blackboard; }
		RGBlackboard& GetBlackboard() { return blackboard; }

	private:
		std::vector<std::unique_ptr<RGPassBase>> passes;
		std::vector<std::unique_ptr<RGTexture>> resources;
		std::vector<RGResourceNode> resource_nodes;
		std::vector<RGResourceView> resource_views;

		std::vector<RGTexture*> imported_resources;
		std::vector<RGResourceView> imported_views;

		std::vector<std::vector<uint64>> adjacency_lists;
		std::vector<RenderGraphPassBase*> topologically_sorted_passes;
		std::vector<DependencyLevel> dependency_levels;

		RGBlackboard blackboard;
		RGResourcePool& pool;
		GraphicsDevice* gfx;
		ResourceViewPool view_pool;
		//std::unique_ptr<Heap> resource_heap; need better allocator then linear if placed resources are used
	private:

		RGResourceHandle CreateResourceNode(RGTexture* resource);
		RGResourceNode& GetResourceNode(RGResourceHandle handle);

		void BuildAdjacencyLists();
		void TopologicalSort();
		void BuildDependencyLevels();
		void CullPasses();
		void CalculateResourcesLifetime();
		void DepthFirstSearch(size_t i, std::vector<bool>& visited, std::stack<size_t>& stack);

		RGResourceView CreateShaderResourceView(RGResourceHandle handle, D3D12_SHADER_RESOURCE_VIEW_DESC  const& desc);
		RGResourceView CreateRenderTargetView(RGResourceHandle handle, D3D12_RENDER_TARGET_VIEW_DESC    const& desc);
		RGResourceView CreateUnorderedAccessView(RGResourceHandle handle, D3D12_UNORDERED_ACCESS_VIEW_DESC const& desc);
		RGResourceView CreateDepthStencilView(RGResourceHandle handle, D3D12_DEPTH_STENCIL_VIEW_DESC    const& desc);
	};

}
