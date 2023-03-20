#include "GfxCommandList.h"
#include "GfxCommandQueue.h"
#include "GfxDevice.h"
#include "GfxBuffer.h"
#include "GfxTexture.h"
#include "GfxPipelineState.h"
#include "RingGPUDescriptorAllocator.h"
#include "LinearDynamicAllocator.h"
#include "../Utilities/StringUtil.h"

namespace adria
{

	GfxCommandList::GfxCommandList(GfxDevice* gfx, GfxCommandListType type /*= GfxCommandListType::Graphics*/, char const* name /*""*/)
		: gfx(gfx), type(type), cmd_queue(gfx->GetCommandQueue(type))
	{
		D3D12_COMMAND_LIST_TYPE cmd_list_type = GetCommandListType(type);
		ID3D12Device* device = gfx->GetDevice();
		HRESULT hr = device->CreateCommandAllocator(cmd_list_type, IID_PPV_ARGS(cmd_allocator.GetAddressOf()));
		BREAK_IF_FAILED(hr);

		hr = device->CreateCommandList(0, cmd_list_type, cmd_allocator, nullptr, IID_PPV_ARGS(cmd_list.GetAddressOf()));
		BREAK_IF_FAILED(hr);

		cmd_list->SetName(ToWideString(name).c_str());
		cmd_list->Close();
	}

	void GfxCommandList::ResetAllocator()
	{
		cmd_allocator->Reset();
	}

	void GfxCommandList::Begin()
	{
		cmd_list->Reset(cmd_allocator.Get(), nullptr);
		ResetState();
	}

	void GfxCommandList::End()
	{
		FlushBarriers();
		cmd_list->Close();
	}

	void GfxCommandList::Wait(GfxFence& fence, uint64 value)
	{
		pending_waits.emplace_back(fence, value);
	}

	void GfxCommandList::Signal(GfxFence& fence, uint64 value)
	{
		pending_signals.emplace_back(fence, value);
	}

	void GfxCommandList::Submit()
	{
		for (size_t i = 0; i < pending_waits.size(); ++i)
		{
			cmd_queue.Wait(pending_waits[i].first, pending_waits[i].second);
		}
		pending_waits.clear();

		if (command_count >= 0) //later > 0
		{
			ID3D12CommandList* cmd_list_array[] = { cmd_list.Get() };
			cmd_queue.ExecuteCommandLists(cmd_list_array);
		}

		for (size_t i = 0; i < pending_signals.size(); ++i)
		{
			cmd_queue.Signal(pending_signals[i].first, pending_signals[i].second);
		}
		pending_signals.clear();
	}

	void GfxCommandList::ResetState()
	{
		command_count = 0;
		current_pso = nullptr;
		current_context = GfxCommandListContext::Invalid;

		if (type == GfxCommandListType::Graphics || type == GfxCommandListType::Compute)
		{
			auto* descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
			if (descriptor_allocator)
			{
				ID3D12DescriptorHeap* heaps[] = { descriptor_allocator->Heap() };
				cmd_list->SetDescriptorHeaps(1, heaps);

				ID3D12RootSignature* common_rs = gfx->GetCommonRootSignature();
				cmd_list->SetComputeRootSignature(common_rs);
				if (type == GfxCommandListType::Graphics) cmd_list->SetGraphicsRootSignature(common_rs);
			}
		}
	}

	void GfxCommandList::Draw(uint32 vertex_count, uint32 instance_count /*= 1*/, uint32 start_vertex_location /*= 0*/, uint32 start_instance_location /*= 0*/)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);
		cmd_list->DrawInstanced(vertex_count, instance_count, start_vertex_location, start_instance_location);
		++command_count;
	}

	void GfxCommandList::DrawIndexed(uint32 index_count, uint32 instance_count /*= 1*/, uint32 index_offset /*= 0*/, uint32 base_vertex_location /*= 0*/, uint32 start_instance_location /*= 0*/)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);
		cmd_list->DrawIndexedInstanced(index_count, instance_count, index_offset, base_vertex_location, start_instance_location);
		++command_count;
	}

	void GfxCommandList::Dispatch(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z /* = 1*/)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Compute);
		cmd_list->Dispatch(group_count_x, group_count_y, group_count_z);
		++command_count;
	}

	void GfxCommandList::DrawIndirect(GfxBuffer* buffer, uint32 offset)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);
		cmd_list->ExecuteIndirect(gfx->GetDrawIndirectSignature(), 1, buffer->GetNative(), offset, nullptr, 0);
		++command_count;
	}

	void GfxCommandList::DrawIndexedIndirect(GfxBuffer* buffer, uint32 offset)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);
		cmd_list->ExecuteIndirect(gfx->GetDrawIndexedIndirectSignature(), 1, buffer->GetNative(), offset, nullptr, 0);
		++command_count;
	}

	void GfxCommandList::DispatchIndirect(GfxBuffer* buffer, uint32 offset)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Compute);
		cmd_list->ExecuteIndirect(gfx->GetDispatchIndirectSignature(), 1, buffer->GetNative(), offset, nullptr, 0);
		++command_count;
	}

	void GfxCommandList::TransitionBarrier(GfxBuffer* resource, GfxResourceState old_state, GfxResourceState new_state)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource->GetNative();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = ConvertToD3D12ResourceState(old_state);
		barrier.Transition.StateAfter = ConvertToD3D12ResourceState(new_state);
		pending_barriers.push_back(barrier);
	}

	void GfxCommandList::TransitionBarrier(GfxTexture* resource, GfxResourceState old_state, GfxResourceState new_state, uint32 subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource->GetNative();
		barrier.Transition.Subresource = subresource;
		barrier.Transition.StateBefore = ConvertToD3D12ResourceState(old_state);
		barrier.Transition.StateAfter = ConvertToD3D12ResourceState(new_state);
		pending_barriers.push_back(barrier);
	}

	void GfxCommandList::UavBarrier(GfxBuffer* resource)
	{
		ADRIA_ASSERT(resource != nullptr);
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = resource->GetNative();
		pending_barriers.push_back(barrier);
	}

	void GfxCommandList::UavBarrier(GfxTexture* resource)
	{
		ADRIA_ASSERT(resource != nullptr);
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = resource->GetNative();
		pending_barriers.push_back(barrier);
	}

	void GfxCommandList::UavBarrier()
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = nullptr;
		pending_barriers.push_back(barrier);
	}

	void GfxCommandList::AliasBarrier(GfxBuffer* before_resource, GfxBuffer* after_resource)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
		barrier.Aliasing.pResourceBefore = before_resource->GetNative();
		barrier.Aliasing.pResourceAfter = after_resource->GetNative();
		pending_barriers.push_back(barrier);
	}

	void GfxCommandList::AliasBarrier(GfxTexture* before_resource, GfxTexture* after_resource)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
		barrier.Aliasing.pResourceBefore = before_resource->GetNative();
		barrier.Aliasing.pResourceAfter = after_resource->GetNative();
		pending_barriers.push_back(barrier);
	}

	void GfxCommandList::FlushBarriers()
	{
		if (!pending_barriers.empty())
		{
			cmd_list->ResourceBarrier((uint32)pending_barriers.size(), pending_barriers.data());
			pending_barriers.clear();
			++command_count;
		}
	}

	void GfxCommandList::CopyBuffer(GfxBuffer* dst, uint32 dst_offset, GfxBuffer* src, uint32 src_offset, uint32 size)
	{
		cmd_list->CopyBufferRegion(dst->GetNative(), dst_offset, src->GetNative(), src_offset, size);
		++command_count;
	}

	void GfxCommandList::CopyBuffer(GfxBuffer* dst, GfxBuffer* src)
	{
		cmd_list->CopyResource(dst->GetNative(), src->GetNative());
		++command_count;
	}

	void GfxCommandList::CopyTexture(GfxTexture* dst, uint32 dst_mip, uint32 dst_array, GfxTexture* src, uint32 src_mip, uint32 src_array)
	{
		D3D12_TEXTURE_COPY_LOCATION dst_texture;
		dst_texture.pResource = dst->GetNative();
		dst_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_texture.SubresourceIndex = dst_mip + dst->GetDesc().mip_levels * dst_array;

		D3D12_TEXTURE_COPY_LOCATION src_texture;
		src_texture.pResource = src->GetNative();
		src_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_texture.SubresourceIndex = src_mip + src->GetDesc().mip_levels * src_array;

		cmd_list->CopyTextureRegion(&dst_texture, 0, 0, 0, &src_texture, nullptr);
		++command_count;
	}

	void GfxCommandList::CopyTexture(GfxTexture* dst, GfxTexture* src)
	{
		cmd_list->CopyResource(dst->GetNative(), src->GetNative());
		++command_count;
	}

	void GfxCommandList::ClearUAV(GfxBuffer* resource, DescriptorHandle uav, const float* clear_value)
	{
		cmd_list->ClearUnorderedAccessViewFloat(uav, uav, resource->GetNative(), clear_value, 0, nullptr);
		++command_count;
	}

	void GfxCommandList::ClearUAV(GfxBuffer* resource, DescriptorHandle uav, const uint32* clear_value)
	{
		cmd_list->ClearUnorderedAccessViewUint(uav, uav, resource->GetNative(), clear_value, 0, nullptr);
		++command_count;
	}

	void GfxCommandList::WriteBufferImmediate(GfxBuffer* buffer, uint32 offset, uint32 data)
	{
		D3D12_WRITEBUFFERIMMEDIATE_PARAMETER parameter{};
		parameter.Dest = buffer->GetGPUAddress() + offset;
		parameter.Value = data;
		cmd_list->WriteBufferImmediate(1, &parameter, nullptr);
		++command_count;
	}

	void GfxCommandList::BeginRenderPass(GfxRenderPassDesc const& render_pass_desc)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);
		ADRIA_ASSERT(current_render_pass == nullptr);
		current_render_pass = std::make_unique<GfxRenderPass>(render_pass_desc);
		current_render_pass->Begin(cmd_list.Get());
	}

	void GfxCommandList::EndRenderPass()
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);
		ADRIA_ASSERT(current_render_pass != nullptr);
		current_render_pass->End(cmd_list.Get());
		current_render_pass.reset();
	}

	void GfxCommandList::SetPipelineState(GfxPipelineState* state)
	{
		if (state != current_pso)
		{
			current_pso = state;
			if (state == nullptr)
			{
				cmd_list->SetPipelineState(nullptr);
				current_context = GfxCommandListContext::Invalid;
			}
			else
			{
				cmd_list->SetPipelineState(*state);
				if (state->GetType() == GfxPipelineStateType::Graphics)
					current_context = GfxCommandListContext::Graphics;
				else current_context = GfxCommandListContext::Compute;
			}
		}
	}

	void GfxCommandList::SetStencilReference(uint8 stencil)
	{
		cmd_list->OMSetStencilRef(stencil);
	}

	void GfxCommandList::SetBlendFactor(float const* blend_factor)
	{
		cmd_list->OMSetBlendFactor(blend_factor);
	}

	void GfxCommandList::SetTopology(GfxPrimitiveTopology topology)
	{
		cmd_list->IASetPrimitiveTopology(ConvertPrimitiveTopology(topology));
	}

	void GfxCommandList::SetIndexBuffer(GfxIndexBufferView* index_buffer_view)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);

		if (index_buffer_view)
		{
			D3D12_INDEX_BUFFER_VIEW ibv{};
			ibv.BufferLocation = index_buffer_view->buffer_location;
			ibv.SizeInBytes = index_buffer_view->size_in_bytes;
			ibv.Format = ConvertGfxFormat(index_buffer_view->format);
			cmd_list->IASetIndexBuffer(&ibv);
		}
		else
		{
			cmd_list->IASetIndexBuffer(nullptr);
		}
	}

	void GfxCommandList::SetVertexBuffers(std::span<GfxVertexBufferView> vertex_buffer_views, uint32 start_slot /*= 0*/)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);

		std::vector<D3D12_VERTEX_BUFFER_VIEW> vbs(vertex_buffer_views.size());
		for (size_t i = 0; i < vertex_buffer_views.size(); ++i)
		{
			vbs[i].BufferLocation = vertex_buffer_views[i].buffer_location;
			vbs[i].SizeInBytes = vertex_buffer_views[i].size_in_bytes;
			vbs[i].StrideInBytes = vertex_buffer_views[i].stride_in_bytes;
		}
		cmd_list->IASetVertexBuffers(start_slot, (uint32)vbs.size(), vbs.data());
	}

	void GfxCommandList::SetViewport(uint32 x, uint32 y, uint32 width, uint32 height)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);

		D3D12_VIEWPORT vp = { (float)x, (float)y, (float)width, (float)height, 0.0f, 1.0f };
		cmd_list->RSSetViewports(1, &vp);
		SetScissorRect(x, y, width, height);
	}

	void GfxCommandList::SetScissorRect(uint32 x, uint32 y, uint32 width, uint32 height)
	{
		ADRIA_ASSERT(current_context == GfxCommandListContext::Graphics);

		D3D12_RECT rect = { (LONG)x, (LONG)y, LONG(x + width), LONG(y + height) };
		cmd_list->RSSetScissorRects(1, &rect);
	}

	void GfxCommandList::SetRootConstants(uint32 slot, const void* data, uint32 data_size, uint32 offset)
	{
		ADRIA_ASSERT(current_context != GfxCommandListContext::Invalid);

		if (current_context == GfxCommandListContext::Graphics)
		{
			cmd_list->SetGraphicsRoot32BitConstants(slot, data_size / sizeof(uint32), data, offset);
		}
		else
		{
			cmd_list->SetComputeRoot32BitConstants(slot, data_size / sizeof(uint32), data, offset);
		}
	}

	void GfxCommandList::SetRootCBV(uint32 slot, const void* data, size_t data_size)
	{
		ADRIA_ASSERT(current_context != GfxCommandListContext::Invalid);

		auto dynamic_allocator = gfx->GetDynamicAllocator();
		DynamicAllocation alloc = dynamic_allocator->Allocate(data_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		alloc.Update(data, data_size);

		if (current_context == GfxCommandListContext::Graphics)
		{
			cmd_list->SetGraphicsRootConstantBufferView(slot, alloc.gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootConstantBufferView(slot, alloc.gpu_address);
		}
	}

	void GfxCommandList::SetRootSRV(uint32 slot, size_t gpu_address)
	{
		ADRIA_ASSERT(current_context != GfxCommandListContext::Invalid);

		if (current_context == GfxCommandListContext::Graphics)
		{
			cmd_list->SetGraphicsRootShaderResourceView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootShaderResourceView(slot, gpu_address);
		}
	}

	void GfxCommandList::SetRootUAV(uint32 slot, size_t gpu_address)
	{
		ADRIA_ASSERT(current_context != GfxCommandListContext::Invalid);

		if (current_context == GfxCommandListContext::Graphics)
		{
			cmd_list->SetGraphicsRootUnorderedAccessView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootUnorderedAccessView(slot, gpu_address);
		}
	}

	void GfxCommandList::SetRootDescriptorTable(uint32 slot, DescriptorHandle base_descriptor)
	{
		ADRIA_ASSERT_MSG(false, "Not yet implemented! (Or needed)");
		if (current_context == GfxCommandListContext::Graphics)
		{
			cmd_list->SetGraphicsRootDescriptorTable(slot, base_descriptor);
		}
		else
		{
			cmd_list->SetComputeRootDescriptorTable(slot, base_descriptor);
		}
	}

	void GfxCommandList::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float const* clear_color)
	{
		cmd_list->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
	}

	void GfxCommandList::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth /*= 1.0f*/, uint8 stencil /*= 0*/, bool clear_stencil /*= false*/)
	{
		D3D12_CLEAR_FLAGS d3d12_clear_flags = D3D12_CLEAR_FLAG_DEPTH;
		if (clear_stencil) d3d12_clear_flags |= D3D12_CLEAR_FLAG_STENCIL;
		cmd_list->ClearDepthStencilView(dsv, d3d12_clear_flags, depth, stencil, 0, nullptr);
	}

	void GfxCommandList::SetRenderTargets(std::span<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs, D3D12_CPU_DESCRIPTOR_HANDLE* dsv /*= nullptr*/, bool single_rt /*= false*/)
	{
		cmd_list->OMSetRenderTargets((uint32)rtvs.size(), rtvs.data(), single_rt, dsv);
	}

}

