#pragma once
#include <d3d12.h>
#include "GfxDescriptor.h"
#include "GfxResourceCommon.h"
#include "GfxStates.h"
#include "GfxRenderPass.h"
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
	struct GfxVertexBufferView;
	struct GfxIndexBufferView;

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
		enum class GfxCommandListContext
		{
			Invalid,
			Graphics,
			Compute
		};
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
		void ResetState();

		void Draw(uint32 vertex_count, uint32 instance_count = 1, uint32 start_vertex_location = 0, uint32 start_instance_location = 0);
		void DrawIndexed(uint32 index_count, uint32 instance_count = 1, uint32 index_offset = 0, uint32 base_vertex_location = 0, uint32 start_instance_location = 0);
		void Dispatch(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z = 1);
		void DrawIndirect(GfxBuffer const& buffer, uint32 offset);
		void DrawIndexedIndirect(GfxBuffer const& buffer, uint32 offset);
		void DispatchIndirect(GfxBuffer const& buffer, uint32 offset);
		//#todo : add Ray Tracing API

		void TransitionBarrier(GfxBuffer const& resource, GfxResourceState old_state, GfxResourceState new_state);
		void TransitionBarrier(GfxTexture const& resource, GfxResourceState old_state, GfxResourceState new_state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		void UavBarrier(GfxBuffer const& resource);
		void UavBarrier(GfxTexture const& resource);
		void UavBarrier();
		void AliasBarrier(GfxBuffer const& before_resource, GfxBuffer const& after_resource);
		void AliasBarrier(GfxTexture const& before_resource, GfxTexture const& after_resource);
		void FlushBarriers();

		void CopyBuffer(GfxBuffer& dst, GfxBuffer const& src);
		void CopyBuffer(GfxBuffer& dst, uint32 dst_offset, GfxBuffer const& src, uint32 src_offset, uint32 size);
		void CopyTexture(GfxTexture& dst, GfxTexture const& src);
		void CopyTexture(GfxTexture& dst, uint32 dst_mip, uint32 dst_array, GfxTexture const& src, uint32 src_mip, uint32 src_array);
		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const float* clear_value);
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const float* clear_value);
		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const uint32* clear_value);
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const uint32* clear_value);
		void WriteBufferImmediate(GfxBuffer& buffer, uint32 offset, uint32 data);

		void BeginRenderPass(GfxRenderPassDesc const& render_pass_desc, bool legacy = false);
		void EndRenderPass(bool legacy = false);

		void SetPipelineState(GfxPipelineState* state);
		void SetStencilReference(uint8 stencil);
		void SetBlendFactor(float const* blend_factor);
		void SetTopology(GfxPrimitiveTopology topology);
		void SetIndexBuffer(GfxIndexBufferView* index_buffer_view);
		void SetVertexBuffers(std::span<GfxVertexBufferView> vertex_buffer_views, uint32 start_slot = 0);
		void SetViewport(uint32 x, uint32 y, uint32 width, uint32 height);
		void SetScissorRect(uint32 x, uint32 y, uint32 width, uint32 height);

		void SetRootConstant(uint32 slot, uint32 data, uint32 offset);
		void SetRootConstants(uint32 slot, const void* data, uint32 data_size, uint32 offset = 0);
		template<typename T>
		void SetRootConstants(uint32 slot, T const& data)
		{
			SetRootConstants(slot, &data, sizeof(T));
		}
		void SetRootCBV(uint32 slot, const void* data, size_t data_size);
		template<typename T>
		void SetRootCBV(uint32 slot, T const& data)
		{
			SetRootCBV(slot, &data, sizeof(T));
		}
		void SetRootCBV(uint32 slot, size_t gpu_address);
		void SetRootSRV(uint32 slot, size_t gpu_address);
		void SetRootUAV(uint32 slot, size_t gpu_address);
		void SetRootDescriptorTable(uint32 slot, GfxDescriptor base_descriptor);

		void ClearRenderTarget(GfxDescriptor rtv, float const* clear_color);
		void ClearDepth(GfxDescriptor dsv, float depth = 1.0f, uint8 stencil = 0, bool clear_stencil = false);
		void SetRenderTargets(std::span<GfxDescriptor> rtvs, GfxDescriptor* dsv = nullptr, bool single_rt = false);

	private:
		GfxDevice* gfx = nullptr;
		GfxCommandListType type;
		GfxCommandQueue& cmd_queue;
		ArcPtr<ID3D12GraphicsCommandList4> cmd_list = nullptr;
		ArcPtr<ID3D12CommandAllocator> cmd_allocator = nullptr;

		uint32 command_count = 0;
		GfxPipelineState* current_pso = nullptr;
		std::unique_ptr<GfxRenderPass> current_render_pass = nullptr;
		GfxCommandListContext current_context = GfxCommandListContext::Invalid;
		std::vector<std::pair<GfxFence&, uint64>> pending_waits;
		std::vector<std::pair<GfxFence&, uint64>> pending_signals;
		std::vector<D3D12_RESOURCE_BARRIER> pending_barriers;
	};
}