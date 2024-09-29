#include "GfxCommandList.h"
#include "GfxCommandQueue.h"
#include "GfxDevice.h"
#include "GfxBuffer.h"
#include "GfxTexture.h"
#include "GfxQueryHeap.h"
#include "GfxPipelineState.h"
#include "GfxRenderPass.h"
#include "GfxRingDescriptorAllocator.h"
#include "GfxLinearDynamicAllocator.h"
#include "GfxRayTracingShaderTable.h"
#include "GfxStateObject.h"
#include "Utilities/StringUtil.h"

namespace adria
{
	namespace
	{
		constexpr D3D_PRIMITIVE_TOPOLOGY ToD3D12PrimitiveTopology(GfxPrimitiveTopology topology)
		{
			switch (topology)
			{
			case GfxPrimitiveTopology::PointList:
				return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			case GfxPrimitiveTopology::LineList:
				return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
			case GfxPrimitiveTopology::LineStrip:
				return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			case GfxPrimitiveTopology::TriangleList:
				return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			case GfxPrimitiveTopology::TriangleStrip:
				return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			default:
				if (topology >= GfxPrimitiveTopology::PatchList1 && topology <= GfxPrimitiveTopology::PatchList32)
					return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + ((uint32)topology - (uint32)GfxPrimitiveTopology::PatchList1));
				else return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
			}
		}
		constexpr D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE ToD3D12RenderPassBeginningAccess(GfxLoadAccessOp op)
		{
			switch (op)
			{
			case GfxLoadAccessOp::Discard:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
			case GfxLoadAccessOp::Preserve:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			case GfxLoadAccessOp::Clear:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			case GfxLoadAccessOp::NoAccess:
				return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
			}
			ADRIA_UNREACHABLE();
		}
		constexpr D3D12_RENDER_PASS_ENDING_ACCESS_TYPE ToD3D12RenderPassEndingAccess(GfxStoreAccessOp op)
		{
			switch (op)
			{
			case GfxStoreAccessOp::Discard:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
			case GfxStoreAccessOp::Preserve:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
			case GfxStoreAccessOp::Resolve:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
			case GfxStoreAccessOp::NoAccess:
				return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			}
			ADRIA_UNREACHABLE();
		}
		constexpr D3D12_RENDER_PASS_FLAGS ToD3D12RenderPassFlags(GfxRenderPassFlags flags)
		{
			D3D12_RENDER_PASS_FLAGS d3d12_flags = D3D12_RENDER_PASS_FLAG_NONE;
			if (flags & GfxRenderPassFlagBit_ReadOnlyDepth) d3d12_flags |= D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH;
			if (flags & GfxRenderPassFlagBit_ReadOnlyStencil) d3d12_flags |= D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_STENCIL;
			if (flags & GfxRenderPassFlagBit_AllowUAVWrites) d3d12_flags |= D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
			if (flags & GfxRenderPassFlagBit_SuspendingPass) d3d12_flags |= D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS;
			if (flags & GfxRenderPassFlagBit_ResumingPass) d3d12_flags |= D3D12_RENDER_PASS_FLAG_RESUMING_PASS;
			return d3d12_flags;
		}
		constexpr D3D12_COMMAND_LIST_TYPE ToD3D12CommandListType(GfxCommandListType type)
		{
			switch (type)
			{
			case GfxCommandListType::Graphics:
				return D3D12_COMMAND_LIST_TYPE_DIRECT;
			case GfxCommandListType::Compute:
				return D3D12_COMMAND_LIST_TYPE_COMPUTE;
			case GfxCommandListType::Copy:
				return D3D12_COMMAND_LIST_TYPE_COPY;
			}
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		}
		constexpr D3D12_QUERY_TYPE ToD3D12QueryType(GfxQueryType query_type)
		{
			switch (query_type)
			{
			case GfxQueryType::Timestamp:
				return D3D12_QUERY_TYPE_TIMESTAMP;
			case GfxQueryType::Occlusion:
				return D3D12_QUERY_TYPE_OCCLUSION;
			case GfxQueryType::BinaryOcclusion:
				return D3D12_QUERY_TYPE_BINARY_OCCLUSION;
			case GfxQueryType::PipelineStatistics:
				return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
			}
			return D3D12_QUERY_TYPE_TIMESTAMP;
		}
		void ToD3D12ClearValue(GfxClearValue value, D3D12_CLEAR_VALUE& d3d12_clear_value)
		{
			d3d12_clear_value.Format = ConvertGfxFormat(value.format);
			if (value.active_member == GfxClearValue::GfxActiveMember::Color)
			{
				memcpy(d3d12_clear_value.Color, value.color.color, sizeof(float) * 4);
			}
			else if(value.active_member == GfxClearValue::GfxActiveMember::DepthStencil)
			{
				d3d12_clear_value.DepthStencil.Depth = value.depth_stencil.depth;
				d3d12_clear_value.DepthStencil.Stencil = value.depth_stencil.stencil;
			}
		}
	}

	GfxCommandList::GfxCommandList(GfxDevice* gfx, GfxCommandListType type, char const* name)
		: gfx(gfx), type(type), cmd_queue(gfx->GetCommandQueue(type)), use_legacy_barriers(!gfx->GetCapabilities().SupportsEnhancedBarriers()), current_rt_table(nullptr)
	{
		D3D12_COMMAND_LIST_TYPE cmd_list_type = ToD3D12CommandListType(type);
		ID3D12Device* device = gfx->GetDevice();
		HRESULT hr = device->CreateCommandAllocator(cmd_list_type, IID_PPV_ARGS(cmd_allocator.GetAddressOf()));
		GFX_CHECK_HR(hr);

		hr = device->CreateCommandList(0, cmd_list_type, cmd_allocator, nullptr, IID_PPV_ARGS(cmd_list.GetAddressOf()));
		GFX_CHECK_HR(hr);

		cmd_list->SetName(ToWideString(name).c_str());
		cmd_list->Close();
	}

	GfxCommandList::~GfxCommandList(){}

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

	void GfxCommandList::WaitAll()
	{
		for (uint64 i = 0; i < pending_waits.size(); ++i)
		{
			cmd_queue.Wait(pending_waits[i].first, pending_waits[i].second);
		}
		pending_waits.clear();
	}

	void GfxCommandList::Submit()
	{
		WaitAll();
		if (command_count >= 0) 
		{
			GfxCommandList* cmd_list_array[] = { this };
			cmd_queue.ExecuteCommandLists(cmd_list_array);
		}
		SignalAll();
	}

	void GfxCommandList::SignalAll()
	{
		for (uint64 i = 0; i < pending_signals.size(); ++i)
		{
			cmd_queue.Signal(pending_signals[i].first, pending_signals[i].second);
		}
		pending_signals.clear();
	}

	void GfxCommandList::ResetState()
	{
		command_count = 0;
		current_pso = nullptr;
		current_render_pass = nullptr;
		current_state_object = nullptr;
		current_rt_table.reset();
		current_context = Context::Invalid;

		if (type == GfxCommandListType::Graphics || type == GfxCommandListType::Compute)
		{
			auto* descriptor_allocator = gfx->GetDescriptorAllocator();
			if (descriptor_allocator)
			{
				ID3D12DescriptorHeap* heaps[] = { descriptor_allocator->GetHeap() };
				cmd_list->SetDescriptorHeaps(1, heaps);

				ID3D12RootSignature* common_rs = gfx->GetCommonRootSignature();
				cmd_list->SetComputeRootSignature(common_rs);
				if (type == GfxCommandListType::Graphics) cmd_list->SetGraphicsRootSignature(common_rs);
			}
		}
	}

	void GfxCommandList::BeginQuery(GfxQueryHeap& query_heap, uint32 index)
	{
		D3D12_QUERY_TYPE d3d12_query_type = ToD3D12QueryType(query_heap.GetDesc().type);
		cmd_list->EndQuery(query_heap, d3d12_query_type, index);
	}

	void GfxCommandList::EndQuery(GfxQueryHeap& query_heap, uint32 index)
	{
		D3D12_QUERY_TYPE d3d12_query_type = ToD3D12QueryType(query_heap.GetDesc().type);
		cmd_list->EndQuery(query_heap, d3d12_query_type, index);
	}

	void GfxCommandList::ResolveQueryData(GfxQueryHeap const& query_heap, uint32 start, uint32 count, GfxBuffer& dst_buffer, uint64 dst_offset)
	{
		cmd_list->ResolveQueryData(query_heap, ToD3D12QueryType(query_heap.GetDesc().type), start, count, dst_buffer.GetNative(), dst_offset);
	}

	void GfxCommandList::Draw(uint32 vertex_count, uint32 instance_count /*= 1*/, uint32 start_vertex_location /*= 0*/, uint32 start_instance_location /*= 0*/)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->DrawInstanced(vertex_count, instance_count, start_vertex_location, start_instance_location);
		++command_count;
	}

	void GfxCommandList::DrawIndexed(uint32 index_count, uint32 instance_count /*= 1*/, uint32 index_offset /*= 0*/, uint32 base_vertex_location /*= 0*/, uint32 start_instance_location /*= 0*/)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->DrawIndexedInstanced(index_count, instance_count, index_offset, base_vertex_location, start_instance_location);
		++command_count;
	}

	void GfxCommandList::Dispatch(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z /* = 1*/)
	{
		ADRIA_ASSERT(current_context == Context::Compute);
		cmd_list->Dispatch(group_count_x, group_count_y, group_count_z);
		++command_count;
	}

	void GfxCommandList::DispatchMesh(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z /*= 1*/)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->DispatchMesh(group_count_x, group_count_y, group_count_z);
		++command_count;
	}

	void GfxCommandList::DrawIndirect(GfxBuffer const& buffer, uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->ExecuteIndirect(gfx->GetDrawIndirectSignature(), 1, buffer.GetNative(), offset, nullptr, 0);
		++command_count;
	}

	void GfxCommandList::DrawIndexedIndirect(GfxBuffer const& buffer, uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->ExecuteIndirect(gfx->GetDrawIndexedIndirectSignature(), 1, buffer.GetNative(), offset, nullptr, 0);
		++command_count;
	}

	void GfxCommandList::DispatchIndirect(GfxBuffer const& buffer, uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Compute);
		cmd_list->ExecuteIndirect(gfx->GetDispatchIndirectSignature(), 1, buffer.GetNative(), offset, nullptr, 0);
		++command_count;
	}

	void GfxCommandList::DispatchMeshIndirect(GfxBuffer const& buffer, uint32 offset)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		cmd_list->ExecuteIndirect(gfx->GetDispatchMeshIndirectSignature(), 1, buffer.GetNative(), offset, nullptr, 0);
		++command_count;
	}

	void GfxCommandList::DispatchRays(uint32 dispatch_width, uint32 dispatch_height, uint32 dispatch_depth /*= 1*/)
	{
		D3D12_DISPATCH_RAYS_DESC dispatch_desc{};
		dispatch_desc.Width = dispatch_width;
		dispatch_desc.Height = dispatch_height;
		dispatch_desc.Depth = dispatch_depth;
		current_rt_table->Commit(*gfx->GetDynamicAllocator(), dispatch_desc);
		cmd_list->DispatchRays(&dispatch_desc);
	}

	void GfxCommandList::TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, uint32 subresource)
	{
		if (use_legacy_barriers)
		{
			if (flags_before == GfxResourceState::ComputeUAV && flags_after == GfxResourceState::ComputeUAV)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = texture.GetNative();
				legacy_barriers.push_back(barrier);
			}
			else
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = texture.GetNative();
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barrier.Transition.StateBefore = ToD3D12LegacyResourceState(flags_before);
				barrier.Transition.StateAfter = ToD3D12LegacyResourceState(flags_after);
				legacy_barriers.push_back(barrier);
			}
		}
		else
		{
			D3D12_TEXTURE_BARRIER barrier{};
			barrier.SyncBefore = ToD3D12BarrierSync(flags_before);
			barrier.SyncAfter = ToD3D12BarrierSync(flags_after);
			barrier.AccessBefore = ToD3D12BarrierAccess(flags_before);
			barrier.AccessAfter = ToD3D12BarrierAccess(flags_after);
			barrier.LayoutBefore = ToD3D12BarrierLayout(flags_before);
			barrier.LayoutAfter = ToD3D12BarrierLayout(flags_after);
			barrier.pResource = texture.GetNative();
			barrier.Subresources = CD3DX12_BARRIER_SUBRESOURCE_RANGE(subresource);

			if (HasAnyFlag(flags_before, GfxResourceState::Discard)) barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
			texture_barriers.push_back(barrier);
		}
	}

	void GfxCommandList::BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after)
	{
		if (use_legacy_barriers)
		{
			if (flags_before == GfxResourceState::ComputeUAV && flags_after == GfxResourceState::ComputeUAV)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = buffer.GetNative();
				legacy_barriers.push_back(barrier);
			}
			else
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = buffer.GetNative();
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barrier.Transition.StateBefore = ToD3D12LegacyResourceState(flags_before);
				barrier.Transition.StateAfter = ToD3D12LegacyResourceState(flags_after);
				legacy_barriers.push_back(barrier);
			}
		}
		else
		{
			D3D12_BUFFER_BARRIER barrier{};
			barrier.SyncBefore = ToD3D12BarrierSync(flags_before);
			barrier.SyncAfter = ToD3D12BarrierSync(flags_after);
			barrier.AccessBefore = ToD3D12BarrierAccess(flags_before);
			barrier.AccessAfter = ToD3D12BarrierAccess(flags_after);
			barrier.pResource = buffer.GetNative();
			barrier.Offset = 0;
			barrier.Size = UINT64_MAX;

			buffer_barriers.push_back(barrier);
		}
	}

	void GfxCommandList::GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after)
	{
		if (use_legacy_barriers)
		{
			if (flags_before == GfxResourceState::ComputeUAV && flags_after == GfxResourceState::ComputeUAV)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = nullptr;
				legacy_barriers.push_back(barrier);
			}
			else
			{
				ADRIA_ASSERT_MSG(false, "Unsupported flags for legacy barriers!");
			}
		}
		else
		{
			D3D12_GLOBAL_BARRIER barrier{};
			barrier.SyncBefore = ToD3D12BarrierSync(flags_before);
			barrier.SyncAfter = ToD3D12BarrierSync(flags_after);
			barrier.AccessBefore = ToD3D12BarrierAccess(flags_before);
			barrier.AccessAfter = ToD3D12BarrierAccess(flags_after);
			global_barriers.push_back(barrier);
		}
	}

	void GfxCommandList::FlushBarriers()
	{
		if (use_legacy_barriers)
		{
			if (!legacy_barriers.empty())
			{
				cmd_list->ResourceBarrier((uint32)legacy_barriers.size(), legacy_barriers.data());
				legacy_barriers.clear();
				++command_count;
			}
		}
		else
		{
			std::vector<D3D12_BARRIER_GROUP> barrier_groups;
			barrier_groups.reserve(3);

			if (!texture_barriers.empty())
			{
				barrier_groups.push_back(CD3DX12_BARRIER_GROUP((uint32)texture_barriers.size(), texture_barriers.data()));
			}

			if (!buffer_barriers.empty())
			{
				barrier_groups.push_back(CD3DX12_BARRIER_GROUP((uint32)buffer_barriers.size(), buffer_barriers.data()));
			}

			if (!global_barriers.empty())
			{
				barrier_groups.push_back(CD3DX12_BARRIER_GROUP((uint32)global_barriers.size(), global_barriers.data()));
			}

			if (!barrier_groups.empty())
			{
				cmd_list->Barrier((uint32)barrier_groups.size(), barrier_groups.data());
				++command_count;
			}

			texture_barriers.clear();
			buffer_barriers.clear();
			global_barriers.clear();
		}
	}

	void GfxCommandList::CopyBuffer(GfxBuffer& dst, uint64 dst_offset, GfxBuffer const& src, uint64 src_offset, uint64 size)
	{
		cmd_list->CopyBufferRegion(dst.GetNative(), dst_offset, src.GetNative(), src_offset, size);
		++command_count;
	}

	void GfxCommandList::CopyBuffer(GfxBuffer& dst, GfxBuffer const& src)
	{
		cmd_list->CopyResource(dst.GetNative(), src.GetNative());
		++command_count;
	}

	void GfxCommandList::CopyTexture(GfxTexture& dst, uint32 dst_mip, uint32 dst_array, GfxTexture const& src, uint32 src_mip, uint32 src_array)
	{
		D3D12_TEXTURE_COPY_LOCATION dst_texture;
		dst_texture.pResource = dst.GetNative();
		dst_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_texture.SubresourceIndex = dst_mip + dst.GetDesc().mip_levels * dst_array;

		D3D12_TEXTURE_COPY_LOCATION src_texture;
		src_texture.pResource = src.GetNative();
		src_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_texture.SubresourceIndex = src_mip + src.GetDesc().mip_levels * src_array;

		cmd_list->CopyTextureRegion(&dst_texture, 0, 0, 0, &src_texture, nullptr);
		++command_count;
	}

	void GfxCommandList::CopyTexture(GfxTexture& dst, GfxTexture const& src)
	{
		cmd_list->CopyResource(dst.GetNative(), src.GetNative());
		++command_count;
	}

	void GfxCommandList::CopyTextureToBuffer(GfxBuffer& dst, uint64 dst_offset, GfxTexture const& src, uint32 src_mip, uint32 src_array)
	{
		GfxTextureDesc const& desc = src.GetDesc();


		D3D12_TEXTURE_COPY_LOCATION dst_texture;
		dst_texture.pResource = dst.GetNative();
		dst_texture.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_texture.PlacedFootprint.Offset = dst_offset;
		dst_texture.PlacedFootprint.Footprint.Width = desc.width;
		dst_texture.PlacedFootprint.Footprint.Depth = 1;
		dst_texture.PlacedFootprint.Footprint.Height = desc.height;
		dst_texture.PlacedFootprint.Footprint.Format = ConvertGfxFormat(desc.format);
		dst_texture.PlacedFootprint.Footprint.RowPitch = (uint32) GetRowPitch(desc.format, dst_texture.PlacedFootprint.Footprint.Width); // (uint32)Align(, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_TEXTURE_COPY_LOCATION src_texture;
		src_texture.pResource = src.GetNative();
		src_texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_texture.SubresourceIndex = src_mip + src.GetDesc().mip_levels * src_array;

		cmd_list->CopyTextureRegion(&dst_texture, (uint32)dst_offset, 0, 0, &src_texture, nullptr);
		++command_count;
	}

	void GfxCommandList::ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const float* clear_value)
	{
		cmd_list->ClearUnorderedAccessViewFloat(uav, uav_cpu, resource.GetNative(), clear_value, 0, nullptr);
		++command_count;
	}

	void GfxCommandList::ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const uint32* clear_value)
	{
		cmd_list->ClearUnorderedAccessViewUint(uav, uav_cpu, resource.GetNative(), clear_value, 0, nullptr);
		++command_count;
	}

	void GfxCommandList::ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const float* clear_value)
	{
		cmd_list->ClearUnorderedAccessViewFloat(uav, uav_cpu, resource.GetNative(), clear_value, 0, nullptr);
		++command_count;
	}

	void GfxCommandList::ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const uint32* clear_value)
	{
		cmd_list->ClearUnorderedAccessViewUint(uav, uav_cpu, resource.GetNative(), clear_value, 0, nullptr);
		++command_count;
	}

	void GfxCommandList::WriteBufferImmediate(GfxBuffer& buffer, uint32 offset, uint32 data)
	{
		D3D12_WRITEBUFFERIMMEDIATE_PARAMETER parameter{};
		parameter.Dest = buffer.GetGpuAddress() + offset;
		parameter.Value = data;
		cmd_list->WriteBufferImmediate(1, &parameter, nullptr);
		++command_count;
	}

	void GfxCommandList::BeginRenderPass(GfxRenderPassDesc const& render_pass_desc)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		ADRIA_ASSERT(current_render_pass == nullptr);
		current_render_pass = &render_pass_desc;
		if (!render_pass_desc.legacy)
		{
			std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> rtvs{};
			std::unique_ptr<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> dsv = nullptr;
			for (auto const& attachment : render_pass_desc.rtv_attachments)
			{
				D3D12_RENDER_PASS_RENDER_TARGET_DESC rtv_desc{};
				rtv_desc.cpuDescriptor = attachment.cpu_handle;
				rtv_desc.BeginningAccess = { ToD3D12RenderPassBeginningAccess(attachment.beginning_access) };
				ToD3D12ClearValue(attachment.clear_value, rtv_desc.BeginningAccess.Clear.ClearValue);
				rtv_desc.EndingAccess = { ToD3D12RenderPassEndingAccess(attachment.ending_access), {} };
				rtvs.push_back(rtv_desc);
			}

			if (render_pass_desc.dsv_attachment)
			{
				auto const& _dsv_desc = render_pass_desc.dsv_attachment.value();
				dsv = std::make_unique<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC>();

				dsv->cpuDescriptor = _dsv_desc.cpu_handle;
				dsv->DepthBeginningAccess = { ToD3D12RenderPassBeginningAccess(_dsv_desc.depth_beginning_access) };
				ToD3D12ClearValue(_dsv_desc.clear_value, dsv->DepthBeginningAccess.Clear.ClearValue);
				dsv->StencilBeginningAccess = { ToD3D12RenderPassBeginningAccess(_dsv_desc.stencil_beginning_access) };
				ToD3D12ClearValue(_dsv_desc.clear_value, dsv->StencilBeginningAccess.Clear.ClearValue);
				dsv->DepthEndingAccess = { ToD3D12RenderPassEndingAccess(_dsv_desc.depth_ending_access), {} };
				dsv->StencilEndingAccess = { ToD3D12RenderPassEndingAccess(_dsv_desc.stencil_ending_access), {} };
				
			}

			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* _dsv = dsv.get();
			D3D12_RENDER_PASS_FLAGS flags = ToD3D12RenderPassFlags(render_pass_desc.flags);
			cmd_list->BeginRenderPass(static_cast<uint32>(rtvs.size()), rtvs.data(), _dsv, flags);
		}
		else
		{
			std::vector<GfxDescriptor> rtv_handles{};
			GfxDescriptor const* dsv_handle = nullptr;

			for (auto const& rtv : render_pass_desc.rtv_attachments)
			{
				rtv_handles.push_back(rtv.cpu_handle);
				if (rtv.beginning_access == GfxLoadAccessOp::Clear) ClearRenderTarget(rtv.cpu_handle, rtv.clear_value.color.color);
			}

			if (render_pass_desc.dsv_attachment.has_value())
			{
				dsv_handle = &render_pass_desc.dsv_attachment->cpu_handle;
				GfxClearValue::GfxClearDepthStencil depth_stencil = render_pass_desc.dsv_attachment->clear_value.depth_stencil;
				if (render_pass_desc.dsv_attachment->depth_beginning_access == GfxLoadAccessOp::Clear)
					ClearDepth(*dsv_handle, depth_stencil.depth, depth_stencil.stencil, false);
			}
			SetRenderTargets(rtv_handles, dsv_handle);
		}
		SetViewport(0, 0, current_render_pass->width, current_render_pass->height);
	}

	void GfxCommandList::EndRenderPass()
	{
		ADRIA_ASSERT(current_context == Context::Graphics);
		ADRIA_ASSERT(current_render_pass != nullptr);
		if (current_render_pass && !current_render_pass->legacy)
		{
			cmd_list->EndRenderPass();
		}
		current_render_pass = nullptr;
	}

	void GfxCommandList::SetPipelineState(GfxPipelineState* state)
	{
		if (state != current_pso)
		{
			current_pso = state;
			if (state == nullptr)
			{
				cmd_list->SetPipelineState(nullptr);
			}
			else
			{
				cmd_list->SetPipelineState(*state);
				if (state->GetType() == GfxPipelineStateType::Graphics || state->GetType() == GfxPipelineStateType::MeshShader)
					ADRIA_ASSERT(current_context == Context::Graphics);
				else ADRIA_ASSERT(current_context == Context::Compute);
			}
		}
	}

	GfxRayTracingShaderTable& GfxCommandList::SetStateObject(GfxStateObject* state_object)
	{
		if (state_object->d3d12_so != current_state_object)
		{
			current_state_object = state_object->d3d12_so;
			cmd_list->SetPipelineState1(state_object->d3d12_so.Get());
			current_context = state_object->d3d12_so ? Context::Compute : Context::Invalid;
			current_rt_table = std::make_unique<GfxRayTracingShaderTable>(state_object);
		}
		return *current_rt_table;
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
		cmd_list->IASetPrimitiveTopology(ToD3D12PrimitiveTopology(topology));
	}

	void GfxCommandList::SetIndexBuffer(GfxIndexBufferView* index_buffer_view)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

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

	void GfxCommandList::SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, uint32 start_slot /*= 0*/)
	{
		D3D12_VERTEX_BUFFER_VIEW vbv{};
		vbv.BufferLocation = vertex_buffer_view.buffer_location;
		vbv.SizeInBytes = vertex_buffer_view.size_in_bytes;
		vbv.StrideInBytes = vertex_buffer_view.stride_in_bytes;
		cmd_list->IASetVertexBuffers(start_slot, 1, &vbv);
	}

	void GfxCommandList::SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, uint32 start_slot /*= 0*/)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

		std::vector<D3D12_VERTEX_BUFFER_VIEW> vbs(vertex_buffer_views.size());
		for (uint64 i = 0; i < vertex_buffer_views.size(); ++i)
		{
			vbs[i].BufferLocation = vertex_buffer_views[i].buffer_location;
			vbs[i].SizeInBytes = vertex_buffer_views[i].size_in_bytes;
			vbs[i].StrideInBytes = vertex_buffer_views[i].stride_in_bytes;
		}
		cmd_list->IASetVertexBuffers(start_slot, (uint32)vbs.size(), vbs.data());
	}

	void GfxCommandList::SetViewport(uint32 x, uint32 y, uint32 width, uint32 height)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

		D3D12_VIEWPORT vp = { (float)x, (float)y, (float)width, (float)height, 0.0f, 1.0f };
		cmd_list->RSSetViewports(1, &vp);
		SetScissorRect(x, y, width, height);
	}

	void GfxCommandList::SetScissorRect(uint32 x, uint32 y, uint32 width, uint32 height)
	{
		ADRIA_ASSERT(current_context == Context::Graphics);

		D3D12_RECT rect = { (LONG)x, (LONG)y, LONG(x + width), LONG(y + height) };
		cmd_list->RSSetScissorRects(1, &rect);
	}

	void GfxCommandList::SetShadingRate(GfxShadingRate shading_rate)
	{
		GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, GfxShadingRateCombiner::Passthrough };
		SetShadingRate(shading_rate, combiners);
	}

	void GfxCommandList::SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners)
	{
		D3D12_SHADING_RATE_COMBINER d3d12_combiners[SHADING_RATE_COMBINER_COUNT] = {};
		for (uint32 i = 0; i < SHADING_RATE_COMBINER_COUNT; ++i)
		{
			d3d12_combiners[i] = ToD3D12ShadingRateCombiner(combiners[i]);
		}
		cmd_list->RSSetShadingRate(ToD3D12ShadingRate(shading_rate), d3d12_combiners);
	}

	void GfxCommandList::SetShadingRateImage(GfxTexture const* texture)
	{
		cmd_list->RSSetShadingRateImage(texture ? texture->GetNative() : nullptr);
	}

	void GfxCommandList::BeginVRS(GfxShadingRateInfo const& info)
	{
		if (info.shading_mode != GfxVariableShadingMode::None)
		{
			if (info.shading_mode == GfxVariableShadingMode::Image && info.shading_rate_image != nullptr &&
				info.shading_rate_combiner != GfxShadingRateCombiner::Passthrough)
			{
				GfxTexture* vrs_image = info.shading_rate_image;
				TextureBarrier(*vrs_image, GfxResourceState::ComputeUAV, GfxResourceState::ShadingRate);
				FlushBarriers();

				GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, info.shading_rate_combiner };
				SetShadingRate(info.shading_rate, combiners);
				SetShadingRateImage(vrs_image);
			}
			else
			{
				GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, info.shading_rate_combiner };
				SetShadingRate(info.shading_rate, combiners);
			}
		}
	}

	void GfxCommandList::EndVRS(GfxShadingRateInfo const& info)
	{
		if (info.shading_mode != GfxVariableShadingMode::None)
		{
			if (info.shading_mode == GfxVariableShadingMode::Image && info.shading_rate_image != nullptr &&
				info.shading_rate_combiner != GfxShadingRateCombiner::Passthrough)
			{
				GfxTexture* vrs_image = info.shading_rate_image;
				TextureBarrier(*vrs_image, GfxResourceState::ShadingRate, GfxResourceState::ComputeUAV);
				FlushBarriers();
			}

			GfxShadingRateCombiner combiners[] = { GfxShadingRateCombiner::Passthrough, GfxShadingRateCombiner::Passthrough };
			SetShadingRate(GfxShadingRate_1X1, combiners);
			SetShadingRateImage(nullptr);
		}
	}

	void GfxCommandList::SetRootConstant(uint32 slot, uint32 data, uint32 offset)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRoot32BitConstant(slot, data, offset);
		}
		else
		{
			cmd_list->SetComputeRoot32BitConstant(slot, data, offset);
		}
	}

	void GfxCommandList::SetRootConstants(uint32 slot, void const* data, uint32 data_size, uint32 offset)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRoot32BitConstants(slot, data_size / sizeof(uint32), data, offset);
		}
		else
		{
			cmd_list->SetComputeRoot32BitConstants(slot, data_size / sizeof(uint32), data, offset);
		}
	}

	void GfxCommandList::SetRootCBV(uint32 slot, void const* data, uint64 data_size)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		auto dynamic_allocator = gfx->GetDynamicAllocator();
		GfxDynamicAllocation alloc = dynamic_allocator->Allocate(data_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		alloc.Update(data, data_size);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootConstantBufferView(slot, alloc.gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootConstantBufferView(slot, alloc.gpu_address);
		}
	}

	void GfxCommandList::SetRootCBV(uint32 slot, uint64 gpu_address)
	{
		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootConstantBufferView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootConstantBufferView(slot, gpu_address);
		}
	}

	void GfxCommandList::SetRootSRV(uint32 slot, uint64 gpu_address)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootShaderResourceView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootShaderResourceView(slot, gpu_address);
		}
	}

	void GfxCommandList::SetRootUAV(uint32 slot, uint64 gpu_address)
	{
		ADRIA_ASSERT(current_context != Context::Invalid);

		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootUnorderedAccessView(slot, gpu_address);
		}
		else
		{
			cmd_list->SetComputeRootUnorderedAccessView(slot, gpu_address);
		}
	}

	void GfxCommandList::SetRootDescriptorTable(uint32 slot, GfxDescriptor base_descriptor)
	{
		if (current_context == Context::Graphics)
		{
			cmd_list->SetGraphicsRootDescriptorTable(slot, base_descriptor);
		}
		else
		{
			cmd_list->SetComputeRootDescriptorTable(slot, base_descriptor);
		}
	}

	GfxDynamicAllocation GfxCommandList::AllocateTransient(uint32 size, uint32 align /*= 0*/)
	{
		return gfx->GetDynamicAllocator()->Allocate(size, align);
	}

	void GfxCommandList::ClearRenderTarget(GfxDescriptor rtv, float const* clear_color)
	{
		cmd_list->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
	}

	void GfxCommandList::ClearDepth(GfxDescriptor dsv, float depth /*= 1.0f*/, uint8 stencil /*= 0*/, bool clear_stencil /*= false*/)
	{
		D3D12_CLEAR_FLAGS d3d12_clear_flags = D3D12_CLEAR_FLAG_DEPTH;
		if (clear_stencil) d3d12_clear_flags |= D3D12_CLEAR_FLAG_STENCIL;
		cmd_list->ClearDepthStencilView(dsv, d3d12_clear_flags, depth, stencil, 0, nullptr);
	}

	void GfxCommandList::SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv /*= nullptr*/, bool single_rt /*= false*/)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE* d3d12_dsv = nullptr;
		if (dsv)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE _d3d12_dsv = *dsv;
			d3d12_dsv = &_d3d12_dsv;
		}
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> d3d12_rtvs(rtvs.size());
		for (uint64 i = 0; i < d3d12_rtvs.size(); ++i) d3d12_rtvs[i] = rtvs[i];
		cmd_list->OMSetRenderTargets((uint32)d3d12_rtvs.size(), d3d12_rtvs.data(), single_rt, d3d12_dsv);
	}

	void GfxCommandList::SetContext(Context ctx)
	{
		current_context = ctx;
	}

}

