#pragma once
#include <span>
#include "GfxDescriptor.h"
#include "GfxResourceCommon.h"
#include "GfxDynamicAllocation.h"
#include "GfxShadingRate.h"
#include "GfxStates.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandQueue;
	class GfxFence;
	class GfxBuffer;
	class GfxTexture;
	class GfxPipelineState;
	class GfxStateObject;
	class GfxQueryHeap;
	struct GfxVertexBufferView;
	struct GfxIndexBufferView;
	struct GfxRenderPassDesc;
	struct GfxShadingRateInfo;
	class GfxRayTracingShaderTable;

	enum class GfxCommandListType : Uint8
	{
		Graphics,
		Compute,
		Copy
	};

	class GfxCommandList
	{
	public:
		enum class Context
		{
			Invalid,
			Graphics,
			Compute
		};

	public:
		explicit GfxCommandList(GfxDevice* gfx, GfxCommandListType type = GfxCommandListType::Graphics, Char const* name = "");
		~GfxCommandList();

		GfxDevice* GetDevice() const { return gfx; }
		ID3D12GraphicsCommandList6* GetNative() const { return cmd_list.Get(); }
		GfxCommandQueue& GetQueue() const { return cmd_queue; }

		void ResetAllocator();
		void Begin();
		void End();
		void Wait(GfxFence& fence, Uint64 value);
		void Signal(GfxFence& fence, Uint64 value);
		void WaitAll();
		void Submit();
		void SignalAll();
		void ResetState();

		void BeginQuery(GfxQueryHeap& query_heap, Uint32 index);
		void EndQuery(GfxQueryHeap& query_heap, Uint32 index);
		void ResolveQueryData(GfxQueryHeap const& query_heap, Uint32 start, Uint32 count, GfxBuffer& dst_buffer, Uint64 dst_offset);

		void Draw(Uint32 vertex_count, Uint32 instance_count = 1, Uint32 start_vertex_location = 0, Uint32 start_instance_location = 0);
		void DrawIndexed(Uint32 index_count, Uint32 instance_count = 1, Uint32 index_offset = 0, Uint32 base_vertex_location = 0, Uint32 start_instance_location = 0);
		void Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1);
		void DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z = 1);
		void DrawIndirect(GfxBuffer const& buffer, Uint32 offset);
		void DrawIndexedIndirect(GfxBuffer const& buffer, Uint32 offset);
		void DispatchIndirect(GfxBuffer const& buffer, Uint32 offset);
		void DispatchMeshIndirect(GfxBuffer const& buffer, Uint32 offset);
		void DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth = 1);

		void TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, Uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		void BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after);
		void GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after);
		void FlushBarriers();

		void CopyBuffer(GfxBuffer& dst, GfxBuffer const& src);
		void CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size);
		void CopyTexture(GfxTexture& dst, GfxTexture const& src);
		void CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array);
		void CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array);

		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const Float* clear_value);
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const Float* clear_value);
		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const Uint32* clear_value);
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const Uint32* clear_value);
		void WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data);

		void BeginRenderPass(GfxRenderPassDesc const& render_pass_desc);
		void EndRenderPass();

		void SetPipelineState(GfxPipelineState* state);
		GfxRayTracingShaderTable& SetStateObject(GfxStateObject* state_object);

		void SetStencilReference(Uint8 stencil);
		void SetBlendFactor(Float const* blend_factor);
		void SetTopology(GfxPrimitiveTopology topology);
		void SetIndexBuffer(GfxIndexBufferView* index_buffer_view);
		void SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, Uint32 start_slot = 0);
		void SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, Uint32 start_slot = 0);
		void SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height);
		void SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height);

		void SetShadingRate(GfxShadingRate shading_rate);
		void SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners);
		void SetShadingRateImage(GfxTexture const* texture);
		void BeginVRS(GfxShadingRateInfo const& info);
		void EndVRS(GfxShadingRateInfo const& info);

		void SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset = 0);
		void SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset = 0);
		template<typename T>
		void SetRootConstants(Uint32 slot, T const& data)
		{
			SetRootConstants(slot, &data, sizeof(T));
		}
		void SetRootCBV(Uint32 slot, void const* data, Uint64 data_size);
		template<typename T>
		void SetRootCBV(Uint32 slot, T const& data)
		{
			SetRootCBV(slot, &data, sizeof(T));
		}
		void SetRootCBV(Uint32 slot, Uint64 gpu_address);
		void SetRootSRV(Uint32 slot, Uint64 gpu_address);
		void SetRootUAV(Uint32 slot, Uint64 gpu_address);
		void SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor);

		GfxDynamicAllocation AllocateTransient(Uint32 size, Uint32 align = 0);

		void ClearRenderTarget(GfxDescriptor rtv, Float const* clear_color);
		void ClearDepth(GfxDescriptor dsv, Float depth = 1.0f, Uint8 stencil = 0, Bool clear_stencil = false);
		void SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv = nullptr, Bool single_rt = false);

		void SetContext(Context ctx);

	private:
		GfxDevice* gfx = nullptr;
		GfxCommandListType type;
		GfxCommandQueue& cmd_queue;
		Ref<ID3D12GraphicsCommandList7> cmd_list = nullptr;
		Ref<ID3D12CommandAllocator> cmd_allocator = nullptr;

		Uint32 command_count = 0;
		GfxPipelineState* current_pso = nullptr;
		GfxRenderPassDesc const* current_render_pass = nullptr;

		ID3D12StateObject* current_state_object = nullptr;
		std::unique_ptr<GfxRayTracingShaderTable> current_rt_table;

		Context current_context = Context::Invalid;

		std::vector<std::pair<GfxFence&, Uint64>> pending_waits;
		std::vector<std::pair<GfxFence&, Uint64>> pending_signals;

		Bool use_legacy_barriers = false;
		std::vector<D3D12_TEXTURE_BARRIER>		  texture_barriers;
		std::vector<D3D12_BUFFER_BARRIER>		  buffer_barriers;
		std::vector<D3D12_GLOBAL_BARRIER>		  global_barriers;
		std::vector<D3D12_RESOURCE_BARRIER>		  legacy_barriers;
	};
}