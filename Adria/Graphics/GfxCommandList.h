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

	enum class GfxCommandListType : uint8
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
		explicit GfxCommandList(GfxDevice* gfx, GfxCommandListType type = GfxCommandListType::Graphics, char const* name = "");
		~GfxCommandList();

		GfxDevice* GetDevice() const { return gfx; }
		ID3D12GraphicsCommandList6* GetNative() const { return cmd_list.Get(); }
		GfxCommandQueue& GetQueue() const { return cmd_queue; }

		void ResetAllocator();
		void Begin();
		void End();
		void Wait(GfxFence& fence, uint64 value);
		void Signal(GfxFence& fence, uint64 value);
		void WaitAll();
		void Submit();
		void SignalAll();
		void ResetState();

		void BeginQuery(GfxQueryHeap& query_heap, uint32 index);
		void EndQuery(GfxQueryHeap& query_heap, uint32 index);
		void ResolveQueryData(GfxQueryHeap const& query_heap, uint32 start, uint32 count, GfxBuffer& dst_buffer, uint64 dst_offset);

		void Draw(uint32 vertex_count, uint32 instance_count = 1, uint32 start_vertex_location = 0, uint32 start_instance_location = 0);
		void DrawIndexed(uint32 index_count, uint32 instance_count = 1, uint32 index_offset = 0, uint32 base_vertex_location = 0, uint32 start_instance_location = 0);
		void Dispatch(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z = 1);
		void DispatchMesh(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z = 1);
		void DrawIndirect(GfxBuffer const& buffer, uint32 offset);
		void DrawIndexedIndirect(GfxBuffer const& buffer, uint32 offset);
		void DispatchIndirect(GfxBuffer const& buffer, uint32 offset);
		void DispatchMeshIndirect(GfxBuffer const& buffer, uint32 offset);
		void DispatchRays(uint32 dispatch_width, uint32 dispatch_height, uint32 dispatch_depth = 1);

		void TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		void BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after);
		void GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after);
		void FlushBarriers();

		void CopyBuffer(GfxBuffer& dst, GfxBuffer const& src);
		void CopyBuffer(GfxBuffer& dst, uint64 dst_offset, GfxBuffer const& src, uint64 src_offset, uint64 size);
		void CopyTexture(GfxTexture& dst, GfxTexture const& src);
		void CopyTexture(GfxTexture& dst, uint32 dst_mip, uint32 dst_array, GfxTexture const& src, uint32 src_mip, uint32 src_array);
		void CopyTextureToBuffer(GfxBuffer& dst, uint64 dst_offset, GfxTexture const& src, uint32 src_mip, uint32 src_array);

		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const float* clear_value);
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const float* clear_value);
		void ClearUAV(GfxBuffer const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const uint32* clear_value);
		void ClearUAV(GfxTexture const& resource, GfxDescriptor uav, GfxDescriptor uav_cpu, const uint32* clear_value);
		void WriteBufferImmediate(GfxBuffer& buffer, uint32 offset, uint32 data);

		void BeginRenderPass(GfxRenderPassDesc const& render_pass_desc);
		void EndRenderPass();

		void SetPipelineState(GfxPipelineState* state);
		GfxRayTracingShaderTable& SetStateObject(GfxStateObject* state_object);

		void SetStencilReference(uint8 stencil);
		void SetBlendFactor(float const* blend_factor);
		void SetTopology(GfxPrimitiveTopology topology);
		void SetIndexBuffer(GfxIndexBufferView* index_buffer_view);
		void SetVertexBuffer(GfxVertexBufferView const& vertex_buffer_view, uint32 start_slot = 0);
		void SetVertexBuffers(std::span<GfxVertexBufferView const> vertex_buffer_views, uint32 start_slot = 0);
		void SetViewport(uint32 x, uint32 y, uint32 width, uint32 height);
		void SetScissorRect(uint32 x, uint32 y, uint32 width, uint32 height);

		void SetShadingRate(GfxShadingRate shading_rate);
		void SetShadingRate(GfxShadingRate shading_rate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT> combiners);
		void SetShadingRateImage(GfxTexture const* texture);
		void BeginVRS(GfxShadingRateInfo const& info);
		void EndVRS(GfxShadingRateInfo const& info);

		void SetRootConstant(uint32 slot, uint32 data, uint32 offset = 0);
		void SetRootConstants(uint32 slot, void const* data, uint32 data_size, uint32 offset = 0);
		template<typename T>
		void SetRootConstants(uint32 slot, T const& data)
		{
			SetRootConstants(slot, &data, sizeof(T));
		}
		void SetRootCBV(uint32 slot, void const* data, uint64 data_size);
		template<typename T>
		void SetRootCBV(uint32 slot, T const& data)
		{
			SetRootCBV(slot, &data, sizeof(T));
		}
		void SetRootCBV(uint32 slot, uint64 gpu_address);
		void SetRootSRV(uint32 slot, uint64 gpu_address);
		void SetRootUAV(uint32 slot, uint64 gpu_address);
		void SetRootDescriptorTable(uint32 slot, GfxDescriptor base_descriptor);

		GfxDynamicAllocation AllocateTransient(uint32 size, uint32 align = 0);

		void ClearRenderTarget(GfxDescriptor rtv, float const* clear_color);
		void ClearDepth(GfxDescriptor dsv, float depth = 1.0f, uint8 stencil = 0, bool clear_stencil = false);
		void SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv = nullptr, bool single_rt = false);

		void SetContext(Context ctx);

	private:
		GfxDevice* gfx = nullptr;
		GfxCommandListType type;
		GfxCommandQueue& cmd_queue;
		Ref<ID3D12GraphicsCommandList7> cmd_list = nullptr;
		Ref<ID3D12CommandAllocator> cmd_allocator = nullptr;

		uint32 command_count = 0;
		GfxPipelineState* current_pso = nullptr;
		GfxRenderPassDesc const* current_render_pass = nullptr;

		ID3D12StateObject* current_state_object = nullptr;
		std::unique_ptr<GfxRayTracingShaderTable> current_rt_table;

		Context current_context = Context::Invalid;

		std::vector<std::pair<GfxFence&, uint64>> pending_waits;
		std::vector<std::pair<GfxFence&, uint64>> pending_signals;

		bool use_legacy_barriers = false;
		std::vector<D3D12_TEXTURE_BARRIER>		  texture_barriers;
		std::vector<D3D12_BUFFER_BARRIER>		  buffer_barriers;
		std::vector<D3D12_GLOBAL_BARRIER>		  global_barriers;
		std::vector<D3D12_RESOURCE_BARRIER>		  legacy_barriers;
	};
}