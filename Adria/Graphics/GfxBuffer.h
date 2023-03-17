#pragma once
#include "GfxResourceCommon.h"

namespace adria
{

	struct GfxBufferDesc
	{
		uint64 size = 0;
		GfxResourceUsage resource_usage = GfxResourceUsage::Default;
		GfxBindFlag bind_flags = GfxBindFlag::None;
		GfxBufferMiscFlag misc_flags = GfxBufferMiscFlag::None;
		uint32 stride = 0; //structured buffers, (vertex buffers, index buffers, needed for count calculation not for srv as structured buffers)
		GfxFormat format = GfxFormat::UNKNOWN; //typed buffers, index buffers
		std::strong_ordering operator<=>(GfxBufferDesc const& other) const = default;
	};

	struct GfxBufferSubresourceDesc
	{
		uint64 offset = 0;
		uint64 size = uint64(-1);
		std::strong_ordering operator<=>(GfxBufferSubresourceDesc const& other) const = default;
	};

	using GfxBufferInitialData = void const*;

	class GfxBuffer
	{
	public:

		GfxBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferInitialData initial_data = nullptr);
		GfxBuffer(GfxBuffer const&) = delete;
		GfxBuffer& operator=(GfxBuffer const&) = delete;
		~GfxBuffer();

		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(size_t i = 0) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(size_t i = 0) const;

		[[maybe_unused]] size_t CreateSRV(GfxBufferSubresourceDesc const* desc = nullptr);
		[[maybe_unused]] size_t CreateUAV(ID3D12Resource* uav_counter = nullptr, GfxBufferSubresourceDesc const* desc = nullptr);
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeSRV(GfxBufferSubresourceDesc const* desc = nullptr);
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeUAV(GfxBufferSubresourceDesc const* desc = nullptr, ID3D12Resource* uav_counter = nullptr);

		ID3D12Resource* GetNative() const;
		ID3D12Resource* Detach();
		D3D12MA::Allocation* DetachAllocation();

		GfxBufferDesc const& GetDesc() const;
		uint32 GetMappedRowPitch() const;
		uint64 GetGPUAddress() const;
		uint32 GetCount() const;

		bool IsMapped() const;
		void* GetMappedData() const;

		template<typename T>
		T* GetMappedData() const;
		[[maybe_unused]] void* Map();

		void Unmap();
		void Update(void const* src_data, size_t data_size, size_t offset = 0);
		template<typename T>
		void Update(T const& src_data);
		void SetName(char const* name);
	private:
		GfxDevice* gfx;
		ArcPtr<ID3D12Resource> resource;
		GfxBufferDesc desc;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srvs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> uavs;

		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;

		void* mapped_data = nullptr;
		uint32 mapped_rowpitch = 0;

	private:

		size_t CreateSubresource(GfxSubresourceType view_type, GfxBufferSubresourceDesc const& view_desc,
			ID3D12Resource* uav_counter = nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource(GfxSubresourceType type, size_t index = 0) const;
	};

	template<typename T>
	T* GfxBuffer::GetMappedData() const
	{
		return reinterpret_cast<T*>(mapped_data);
	}
	template<typename T>
	void GfxBuffer::Update(T const& src_data)
	{
		Update(&src_data, sizeof(T));
	}

	inline void BindVertexBuffer(ID3D12GraphicsCommandList* cmd_list, GfxBuffer const* vertex_buffer)
	{
		D3D12_VERTEX_BUFFER_VIEW vb_view{};
		vb_view.BufferLocation = vertex_buffer->GetGPUAddress();
		vb_view.SizeInBytes = (UINT)vertex_buffer->GetDesc().size;
		vb_view.StrideInBytes = vertex_buffer->GetDesc().stride;
		cmd_list->IASetVertexBuffers(0, 1, &vb_view);
	}
	inline void BindIndexBuffer(ID3D12GraphicsCommandList* cmd_list, GfxBuffer const* index_buffer)
	{
		D3D12_INDEX_BUFFER_VIEW ib_view{};
		ib_view.BufferLocation = index_buffer->GetGPUAddress();
		ib_view.Format = ConvertGfxFormat(index_buffer->GetDesc().format);
		ib_view.SizeInBytes = (UINT)index_buffer->GetDesc().size;
		cmd_list->IASetIndexBuffer(&ib_view);
	}
	inline GfxBufferDesc VertexBufferDesc(uint64 vertex_count, uint32 stride, bool ray_tracing = true)
	{
		GfxBufferDesc desc{};
		desc.bind_flags = ray_tracing ? GfxBindFlag::ShaderResource : GfxBindFlag::None;
		desc.resource_usage = GfxResourceUsage::Default;
		desc.size = vertex_count * stride;
		desc.stride = stride;
		return desc;
	}
	inline GfxBufferDesc IndexBufferDesc(uint64 index_count, bool small_indices, bool ray_tracing = true)
	{
		GfxBufferDesc desc{};
		desc.bind_flags = ray_tracing ? GfxBindFlag::ShaderResource : GfxBindFlag::None;
		desc.resource_usage = GfxResourceUsage::Default;
		desc.stride = small_indices ? 2 : 4;
		desc.size = index_count * desc.stride;
		desc.format = small_indices ? GfxFormat::R16_UINT : GfxFormat::R32_UINT;
		return desc;
	}
	inline GfxBufferDesc ReadBackBufferDesc(uint64 size)
	{
		GfxBufferDesc desc{};
		desc.bind_flags = GfxBindFlag::None;
		desc.resource_usage = GfxResourceUsage::Readback;
		desc.size = size;
		desc.misc_flags = GfxBufferMiscFlag::None;
		return desc;
	}
	template<typename T>
	inline GfxBufferDesc StructuredBufferDesc(uint64 count, bool uav = true, bool dynamic = false)
	{
		GfxBufferDesc desc{};
		desc.resource_usage = (uav || !dynamic) ? GfxResourceUsage::Default : GfxResourceUsage::Upload;
		desc.bind_flags = GfxBindFlag::ShaderResource;
		if (uav) desc.bind_flags |= GfxBindFlag::UnorderedAccess;
		desc.misc_flags = GfxBufferMiscFlag::BufferStructured;
		desc.stride = sizeof(T);
		desc.size = desc.stride * count;
		return desc;
	}
	inline GfxBufferDesc CounterBufferDesc()
	{
		GfxBufferDesc desc{};
		desc.size = sizeof(uint32);
		desc.bind_flags = GfxBindFlag::UnorderedAccess;
		return desc;
	}

	struct GfxVertexBufferView
	{
		explicit GfxVertexBufferView(GfxBuffer* buffer)
			: buffer_location(buffer->GetGPUAddress()), size_in_bytes((uint32)buffer->GetDesc().size), stride_in_bytes(buffer->GetDesc().stride)
		{}

		GfxVertexBufferView(uint64 buffer_location, uint32 size_in_bytes, uint32 stride_in_bytes)
			: buffer_location(buffer_location), size_in_bytes(size_in_bytes), stride_in_bytes(stride_in_bytes)
		{}

		uint64					    buffer_location = 0;
		uint32                      size_in_bytes = 0;
		uint32                      stride_in_bytes = 0;
	};

	struct GfxIndexBufferView
	{
		explicit GfxIndexBufferView(GfxBuffer* buffer)
			: buffer_location(buffer->GetGPUAddress()), size_in_bytes((uint32)buffer->GetDesc().size), format(buffer->GetDesc().format)
		{}

		GfxIndexBufferView(uint64 buffer_location, uint32 size_in_bytes, GfxFormat format = GfxFormat::R32_UINT)
			: buffer_location(buffer_location), size_in_bytes(size_in_bytes), format(format)
		{}

		uint64					    buffer_location = 0;
		uint32                      size_in_bytes;
		GfxFormat                   format;
	};

}