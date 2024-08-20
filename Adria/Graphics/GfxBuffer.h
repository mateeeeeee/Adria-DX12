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
		uint32 stride = 0; 
		GfxFormat format = GfxFormat::UNKNOWN; 
		std::strong_ordering operator<=>(GfxBufferDesc const& other) const = default;
	};

	struct GfxBufferDescriptorDesc
	{
		uint64 offset = 0;
		uint64 size = uint64(-1);
		std::strong_ordering operator<=>(GfxBufferDescriptorDesc const& other) const = default;
	};

	struct GfxBufferData
	{
		GfxBufferData() {}
		GfxBufferData(void const* _data) : data(_data) {}
		operator void const* () const { return data; }

		void const* data = nullptr;
	};

	class GfxBuffer
	{
	public:

		GfxBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data = {});
		ADRIA_NONCOPYABLE_NONMOVABLE(GfxBuffer)
		~GfxBuffer();

		ID3D12Resource* GetNative() const;

		GfxBufferDesc const& GetDesc() const;
		uint64 GetGpuAddress() const;
		uint64 GetSize() const;
		uint32 GetStride() const;
		uint32 GetCount() const;
		GfxFormat GetFormat() const;

		bool IsMapped() const;
		void* GetMappedData() const;
		template<typename T>
		T* GetMappedData() const;
		ADRIA_MAYBE_UNUSED void* Map();
		void Unmap();
		void Update(void const* src_data, uint64 data_size, uint64 offset = 0);
		template<typename T>
		void Update(T const& src_data);

		void SetName(char const* name);

	private:
		GfxDevice* gfx;
		Ref<ID3D12Resource> resource;
		GfxBufferDesc desc;
		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		void* mapped_data = nullptr;
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

	struct GfxVertexBufferView
	{
		explicit GfxVertexBufferView(GfxBuffer* buffer)
			: buffer_location(buffer->GetGpuAddress()), size_in_bytes((uint32)buffer->GetSize()), stride_in_bytes(buffer->GetStride())
		{}

		GfxVertexBufferView(uint64 buffer_location, uint32 count, uint32 stride_in_bytes)
			: buffer_location(buffer_location), size_in_bytes(count * stride_in_bytes), stride_in_bytes(stride_in_bytes)
		{}

		uint64					    buffer_location = 0;
		uint32                      size_in_bytes = 0;
		uint32                      stride_in_bytes = 0;
	};

	struct GfxIndexBufferView
	{
		explicit GfxIndexBufferView(GfxBuffer* buffer)
			: buffer_location(buffer->GetGpuAddress()), size_in_bytes((uint32)buffer->GetSize()), format(buffer->GetFormat())
		{}

		GfxIndexBufferView(uint64 buffer_location, uint32 count, GfxFormat format = GfxFormat::R32_UINT)
			: buffer_location(buffer_location), size_in_bytes(count * GetGfxFormatStride(format)), format(format)
		{}

		uint64					    buffer_location = 0;
		uint32                      size_in_bytes;
		GfxFormat                   format;
	};

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
}