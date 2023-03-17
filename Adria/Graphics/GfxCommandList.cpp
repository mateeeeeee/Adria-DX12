#include "GfxCommandList.h"
#include "GfxCommandQueue.h"
#include "GfxDevice.h"
#include "../Utilities/StringUtil.h"

namespace adria
{

	GfxCommandList::GfxCommandList(GfxDevice* gfx, GfxCommandListType type /*= GfxCommandListType::Graphics*/, char const* name /*""*/)
		: gfx(gfx), cmd_queue(gfx->GetCommandQueue(type))
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

	}

	void GfxCommandList::End()
	{

	}

	void GfxCommandList::Wait(GfxFence& fence, uint64 value)
	{

	}

	void GfxCommandList::Signal(GfxFence& fence, uint64 value)
	{

	}

	void GfxCommandList::Submit()
	{

	}

	void GfxCommandList::ClearState()
	{

	}

	void GfxCommandList::BeginEvent(char const* name)
	{

	}

	void GfxCommandList::EndEvent()
	{

	}

	void GfxCommandList::Draw(uint32 vertex_count, uint32 instance_count /*= 1*/)
	{

	}

	void GfxCommandList::DrawIndexed(uint32 index_count, uint32 instance_count /*= 1*/, uint32 index_offset /*= 0*/)
	{

	}

	void GfxCommandList::Dispatch(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z)
	{

	}

	void GfxCommandList::DrawIndirect(GfxBuffer* buffer, uint32 offset)
	{

	}

	void GfxCommandList::DrawIndexedIndirect(GfxBuffer* buffer, uint32 offset)
	{

	}

	void GfxCommandList::DispatchIndirect(GfxBuffer* buffer, uint32 offset)
	{

	}

	void GfxCommandList::ResourceBarrier(GfxBuffer* resource, GfxResourceState old_state, GfxResourceState new_state)
	{

	}

	void GfxCommandList::ResourceBarrier(GfxTexture* resource, GfxResourceState old_state, GfxResourceState new_state, uint32 subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
	{

	}

	void GfxCommandList::UavBarrier(GfxBuffer* resource)
	{

	}

	void GfxCommandList::UavBarrier(GfxTexture* resource)
	{

	}

	void GfxCommandList::AliasBarrier(GfxBuffer* before_resource, GfxBuffer* after_resource)
	{

	}

	void GfxCommandList::AliasBarrier(GfxTexture* before_resource, GfxTexture* after_resource)
	{

	}

	void GfxCommandList::FlushBarriers()
	{

	}

	void GfxCommandList::CopyBufferToTexture(GfxTexture* dst_texture, uint32 mip_level, uint32 array_slice, GfxBuffer* src_buffer, uint32 offset)
	{

	}

	void GfxCommandList::CopyTextureToBuffer(GfxBuffer* dst_buffer, GfxTexture* src_texture, uint32 mip_level, uint32 array_slice)
	{

	}

	void GfxCommandList::CopyBuffer(GfxBuffer* dst, uint32 dst_offset, GfxBuffer* src, uint32 src_offset, uint32 size)
	{

	}

	void GfxCommandList::CopyTexture(GfxTexture* dst, uint32 dst_mip, uint32 dst_array, GfxTexture* src, uint32 src_mip, uint32 src_array)
	{

	}

	void GfxCommandList::ClearUAV(GfxBuffer* resource, DescriptorHandle uav, const float* clear_value)
	{

	}

	void GfxCommandList::ClearUAV(GfxBuffer* resource, DescriptorHandle uav, const uint32* clear_value)
	{

	}

	void GfxCommandList::WriteImmediateBuffer(GfxBuffer* buffer, uint32 offset, uint32 data)
	{

	}

}

