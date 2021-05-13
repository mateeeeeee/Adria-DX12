#pragma once
#include <wrl.h>
#include <mutex>
#include <map>
#include <unordered_map>
#include "ResourceBarriers.h"
#include "d3dx12.h"
#include "../Core/Macros.h"

//https://github.com/jpvanoosten/LearningDirectX12/blob/v0.0.3/DX12Lib/src/ResourceStateTracker.cpp


namespace adria
{

    

    class ResourceStateTracker
    {
    public:
        ResourceStateTracker() {};
        ~ResourceStateTracker() {};

        /**
         * Push a resource barrier to the resource state tracker.
         *
         * @param barrier The resource barrier to push to the resource state tracker.
         */
        void AddResourceBarrier(D3D12_RESOURCE_BARRIER const& barrier)
        {
            if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
            {
                const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;

                // First check if there is already a known "final" state for the given resource.
                // If there is, the resource has been used on the command list before and
                // already has a known state within the command list execution.
                const auto iter = local_resource_state_map.find(transitionBarrier.pResource);
                if (iter != local_resource_state_map.end())
                {
                    auto& resourceState = iter->second;
                    // If the known final state of the resource is different...
                    if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
                        !resourceState.SubresourceState.empty())
                    {
                        // First transition all of the subresources if they are different than the StateAfter.
                        for (auto subresourceState : resourceState.SubresourceState)
                        {
                            if (transitionBarrier.StateAfter != subresourceState.second)
                            {
                                D3D12_RESOURCE_BARRIER newBarrier = barrier;
                                newBarrier.Transition.Subresource = subresourceState.first;
                                newBarrier.Transition.StateBefore = subresourceState.second;
                                resource_barriers.push_back(newBarrier);
                            }
                        }
                    }
                    else
                    {
                        auto finalState = resourceState.GetSubresourceState(transitionBarrier.Subresource);
                        if (transitionBarrier.StateAfter != finalState)
                        {
                            // Push a new transition barrier with the correct before state.
                            D3D12_RESOURCE_BARRIER newBarrier = barrier;
                            newBarrier.Transition.StateBefore = finalState;
                            resource_barriers.push_back(newBarrier);
                        }
                    }
                }
                else // In this case, the resource is being used on the command list for the first time. 
                {
                    // Add a pending barrier. The pending barriers will be resolved
                    // before the command list is executed on the command queue.
                    pending_resource_barriers.push_back(barrier);
                }

                // Push the final known state (possibly replacing the previously known state for the subresource).
                local_resource_state_map[transitionBarrier.pResource].SetSubresourceState(transitionBarrier.Subresource, transitionBarrier.StateAfter);
            }
            else
            {
                // Just push non-transition barriers to the resource barriers array.
                resource_barriers.push_back(barrier);
            }
        }

        /**
         * Push a transition resource barrier to the resource state tracker.
         *
         * @param resource The resource to transition.
         * @param stateAfter The state to transition the resource to.
         * @param subResource The subresource to transition. By default, this is D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
         * which indicates that all subresources should be transitioned to the same state.
         */
        void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        {
            if (resource)
            {
                AddResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, stateAfter, subResource));
            }
        }
        
        /**
         * Push a UAV resource barrier for the given resource.
         *
         * @param resource The resource to add a UAV barrier for. Can be NULL which
         * indicates that any UAV access could require the barrier.
         */
        void UAVBarrier(ID3D12Resource* resource = nullptr)
        {
            AddResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(resource));
        }

        /**
         * Push an aliasing barrier for the given resource.
         *
         * @param beforeResource The resource currently occupying the space in the heap.
         * @param afterResource The resource that will be occupying the space in the heap.
         *
         * Either the beforeResource or the afterResource parameters can be NULL which
         * indicates that any placed or reserved resource could cause aliasing.
         */
        void AliasBarrier(ID3D12Resource* resourceBefore = nullptr, ID3D12Resource* resourceAfter = nullptr)
        {
            AddResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(resourceBefore, resourceAfter));
        }

        /**
         * Flush any pending resource barriers to the command list.
         *
         * @return The number of resource barriers that were flushed to the command list.
         */
        uint32_t FlushPendingResourceBarriers(ID3D12GraphicsCommandList* commandList)
        {
            ADRIA_ASSERT(is_locked);
            // 
            // 
            // Resolve the pending resource barriers by checking the global state of the 
            // (sub)resources. Add barriers if the pending state and the global state do
            //  not match.
            ResourceBarriers resourceBarriers;
            // Reserve enough space (worst-case, all pending barriers).
            resourceBarriers.reserve(pending_resource_barriers.size());

            for (auto pendingBarrier : pending_resource_barriers)
            {
                if (pendingBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)  // Only transition barriers should be pending...
                {
                    auto pendingTransition = pendingBarrier.Transition;

                    const auto& iter = global_resource_state_map.find(pendingTransition.pResource);
                    if (iter != global_resource_state_map.end())
                    {
                        // If all subresources are being transitioned, and there are multiple
                        // subresources of the resource that are in a different state...
                        auto& resourceState = iter->second;
                        if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
                            !resourceState.SubresourceState.empty())
                        {
                            // Transition all subresources
                            for (auto subresourceState : resourceState.SubresourceState)
                            {
                                if (pendingTransition.StateAfter != subresourceState.second)
                                {
                                    D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
                                    newBarrier.Transition.Subresource = subresourceState.first;
                                    newBarrier.Transition.StateBefore = subresourceState.second;
                                    resourceBarriers.push_back(newBarrier);
                                }
                            }
                        }
                        else
                        {
                            // No (sub)resources need to be transitioned. Just add a single transition barrier (if needed).
                            auto globalState = (iter->second).GetSubresourceState(pendingTransition.Subresource);
                            if (pendingTransition.StateAfter != globalState)
                            {
                                // Fix-up the before state based on current global state of the resource.
                                pendingBarrier.Transition.StateBefore = globalState;
                                resourceBarriers.push_back(pendingBarrier);
                            }
                        }
                    }
                }
            }

            UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
            if (numBarriers > 0)
            {
               
                commandList->ResourceBarrier(numBarriers, resourceBarriers.data());
            }

            pending_resource_barriers.clear();

            return numBarriers;
        }

        /**
         * Flush any (non-pending) resource barriers that have been pushed to the resource state
         * tracker.
         */
        void FlushResourceBarriers(ID3D12GraphicsCommandList* commandList)
        {
            UINT numBarriers = static_cast<UINT>(resource_barriers.size());
            if (numBarriers > 0)
            {
                commandList->ResourceBarrier(numBarriers, resource_barriers.data());
                resource_barriers.clear();
            }
        }

        /**
         * Commit final resource states to the global resource state map.
         * This must be called when the command list is closed.
         */
        void CommitFinalResourceStates()
        {
            ADRIA_ASSERT(is_locked);

            // Commit final resource states to the global resource state array (map).
            for (const auto& resourceState : local_resource_state_map)
            {
                global_resource_state_map[resourceState.first] = resourceState.second;
            }

            local_resource_state_map.clear();
        }

        /**
         * Reset state tracking. This must be done when the command list is reset.
         */
        void Reset()
        {
            // Reset the pending, current, and final resource states.
            pending_resource_barriers.clear();
            resource_barriers.clear();
            local_resource_state_map.clear();
        }

        /**
         * The global state must be locked before flushing pending resource barriers
         * and committing the final resource state to the global resource state.
         * This ensures consistency of the global resource state between command list
         * executions.
         */
        static void Lock()
        {
            global_mutex.lock();
            is_locked = true;
        }

        /**
         * Unlocks the global resource state after the final states have been committed
         * to the global resource state array.
         */
        static void Unlock()
        {
            global_mutex.unlock();
           
            is_locked = false;
        }

        /**
         * Add a resource with a given state to the global resource state array (map).
         * This should be done when the resource is created for the first time.
         */
        static void AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
        {
            if (resource != nullptr)
            {
                std::lock_guard<std::mutex> lock(global_mutex);
                global_resource_state_map[resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
            }
        }

        /**
         * Remove a resource from the global resource state array (map).
         * This should only be done when the resource is destroyed.
         */
        static void RemoveGlobalResourceState(ID3D12Resource* resource)
        {
            if (resource != nullptr)
            {
                std::lock_guard<std::mutex> lock(global_mutex);
                global_resource_state_map.erase(resource);
            }
        }

    protected:

    private:
        // An array (vector) of resource barriers.
        using ResourceBarriers = std::vector<D3D12_RESOURCE_BARRIER>;

        // Pending resource transitions are committed before a command list
        // is executed on the command queue. This guarantees that resources will
        // be in the expected state at the beginning of a command list.
        ResourceBarriers pending_resource_barriers;

        // Resource barriers that need to be committed to the command list.
        ResourceBarriers resource_barriers;

        // Tracks the state of a particular resource and all of its subresources.
        struct ResourceState
        {
            // Initialize all of the subresources within a resource to the given state.
            explicit ResourceState(D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON)
                : State(state)
            {}

            // Set a subresource to a particular state.
            void SetSubresourceState(UINT subresource, D3D12_RESOURCE_STATES state)
            {
                if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
                {
                    State = state;
                    SubresourceState.clear();
                }
                else
                {
                    SubresourceState[subresource] = state;
                }
            }

            // Get the state of a (sub)resource within the resource.
            // If the specified subresource is not found in the SubresourceState array (map)
            // then the state of the resource (D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) is
            // returned.
            D3D12_RESOURCE_STATES GetSubresourceState(UINT subresource) const
            {
                D3D12_RESOURCE_STATES state = State;
                const auto iter = SubresourceState.find(subresource);
                if (iter != SubresourceState.end())
                {
                    state = iter->second;
                }
                return state;
            }

            // If the SubresourceState array (map) is empty, then the State variable defines 
            // the state of all of the subresources.
            D3D12_RESOURCE_STATES State;
            std::map<UINT, D3D12_RESOURCE_STATES> SubresourceState;
        };

        using ResourceStateMap = std::unordered_map<ID3D12Resource*, ResourceState>;

        // The final (last known state) of the resources within a command list.
        // The final resource state is committed to the global resource state when the 
        // command list is closed but before it is executed on the command queue.
        ResourceStateMap local_resource_state_map;

        inline static ResourceStateMap global_resource_state_map{};

        inline static std::mutex global_mutex{};
        inline static bool is_locked = false;
    };
}