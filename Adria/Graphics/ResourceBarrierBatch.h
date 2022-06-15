#pragma once
#include <d3d12.h>
#include <vector>


namespace adria
{
	class ResourceBarrierBatch
	{
	public:

		ResourceBarrierBatch() = default;

		void AddTransition(
			ID3D12Resource* resource,
			D3D12_RESOURCE_STATES state_before,
			D3D12_RESOURCE_STATES state_after,
			UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		{
			D3D12_RESOURCE_BARRIER resource_barrier{};
			resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			resource_barrier.Transition.pResource = resource;
			resource_barrier.Transition.StateBefore = state_before;
			resource_barrier.Transition.StateAfter = state_after;
			resource_barrier.Transition.Subresource = subresource;
			resource_barriers.push_back(resource_barrier);
		}

		void AddSplitTransition(ID3D12Resource* resource,
			D3D12_RESOURCE_STATES state_before,
			D3D12_RESOURCE_STATES state_after,
			bool end_flag = false,
			UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		{
			D3D12_RESOURCE_BARRIER resource_barrier{};
			resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			resource_barrier.Flags = end_flag ? D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY : D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
			resource_barrier.Transition.pResource = resource;
			resource_barrier.Transition.StateBefore = state_before;
			resource_barrier.Transition.StateAfter = state_after;
			resource_barrier.Transition.Subresource = subresource;
			resource_barriers.push_back(resource_barrier);
		}

		void AddUAV(ID3D12Resource* resource)
		{
			D3D12_RESOURCE_BARRIER resource_barrier = {};
			resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			resource_barrier.UAV.pResource = resource;
			resource_barriers.push_back(resource_barrier);
		}

		void AddAliasing(ID3D12Resource* before, ID3D12Resource* after)
		{
			D3D12_RESOURCE_BARRIER resource_barrier = {};
			resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			resource_barrier.Aliasing.pResourceBefore = before;
			resource_barrier.Aliasing.pResourceAfter = after;
			resource_barriers.push_back(resource_barrier);
		}

		void ReverseTransitions()
		{
			for (auto& resource_barrier : resource_barriers)
			{
				if (resource_barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
					std::swap(resource_barrier.Transition.StateBefore, resource_barrier.Transition.StateAfter);
			}
		}

		void EndSplitTransitions()
		{
			for (auto& resource_barrier : resource_barriers)
			{
				if (resource_barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION
					&& resource_barrier.Flags == D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
					resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
			}
		}

		void Submit(ID3D12GraphicsCommandList* command_list) const
		{
			if(!resource_barriers.empty()) command_list->ResourceBarrier(static_cast<UINT>(resource_barriers.size()), resource_barriers.data());
		}

		void Merge(ResourceBarrierBatch&& barriers)
		{
			for (auto&& barrier : barriers.resource_barriers)
				resource_barriers.emplace_back(barrier);
		}

		void Clear()
		{
			resource_barriers.clear();
		}

	private:
		std::vector<D3D12_RESOURCE_BARRIER> resource_barriers;
	};
}