#pragma once
#include <d3d12.h>
#include "DescriptorHeap.h"
#include "GfxResourceCommon.h"
#include "../Utilities/AutoRefCountPtr.h"
#include "../Core/Definitions.h"


namespace adria
{
	class GfxDevice;
	class GfxCommandQueue;
	class GfxFence;
	class GfxBuffer;
	class GfxTexture;
	class GfxPipelineState;

	enum class GfxCommandListType : uint8
	{
		Graphics,
		Compute,
		Copy
	};

	inline constexpr D3D12_COMMAND_LIST_TYPE GetCommandListType(GfxCommandListType type)
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

	class GfxCommandList
	{
	public:
		explicit GfxCommandList(GfxDevice* gfx, GfxCommandListType type = GfxCommandListType::Graphics, char const* name = "");

		ID3D12GraphicsCommandList4* GetNative() const { return cmd_list.Get(); }
		GfxCommandQueue& GetQueue() const { return cmd_queue; }

		void ResetAllocator();
		void Begin();
		void End();
		void Wait(GfxFence& fence, uint64 value);
		void Signal(GfxFence& fence, uint64 value);
		void Submit();
		void ClearState();

		void BeginEvent(char const* name);
		void EndEvent();

		void Draw(uint32 vertex_count, uint32 instance_count = 1);
		void DrawIndexed(uint32 index_count, uint32 instance_count = 1, uint32 index_offset = 0);
		void Dispatch(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z);
		void DrawIndirect(GfxBuffer* buffer, uint32 offset);
		void DrawIndexedIndirect(GfxBuffer* buffer, uint32 offset);
		void DispatchIndirect(GfxBuffer* buffer, uint32 offset);

		void ResourceBarrier(GfxBuffer* resource, GfxResourceState old_state, GfxResourceState new_state);
		void ResourceBarrier(GfxTexture* resource, GfxResourceState old_state, GfxResourceState new_state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		void UavBarrier(GfxBuffer* resource);
		void UavBarrier(GfxTexture* resource);
		void AliasBarrier(GfxBuffer* before_resource, GfxBuffer* after_resource);
		void AliasBarrier(GfxTexture* before_resource, GfxTexture* after_resource);
		void FlushBarriers();

		void CopyBufferToTexture(GfxTexture* dst_texture, uint32 mip_level, uint32 array_slice, GfxBuffer* src_buffer, uint32 offset);
		void CopyTextureToBuffer(GfxBuffer* dst_buffer, GfxTexture* src_texture, uint32 mip_level, uint32 array_slice);
		void CopyBuffer(GfxBuffer* dst, uint32 dst_offset, GfxBuffer* src, uint32 src_offset, uint32 size);
		void CopyTexture(GfxTexture* dst, uint32 dst_mip, uint32 dst_array, GfxTexture* src, uint32 src_mip, uint32 src_array);
		void ClearUAV(GfxBuffer* resource, DescriptorHandle uav, const float* clear_value);
		void ClearUAV(GfxBuffer* resource, DescriptorHandle uav, const uint32* clear_value);
		void WriteImmediateBuffer(GfxBuffer* buffer, uint32 offset, uint32 data);

	private:
		GfxDevice* gfx = nullptr;
		GfxCommandQueue& cmd_queue;
		ArcPtr<ID3D12GraphicsCommandList4> cmd_list;
		ArcPtr<ID3D12CommandAllocator> cmd_allocator;

		uint32 command_count;
		std::vector<D3D12_RESOURCE_BARRIER> resource_barriers;
		std::vector<std::pair<GfxFence&, uint64>> pending_waits;
		std::vector<std::pair<GfxFence&, uint64>> pending_signals;
	};
}