#pragma once
#include "ResourceCommon.h"

namespace adria
{
	
	struct BufferDesc
	{
		uint64 size = 0;
		EResourceUsage resource_usage = EResourceUsage::Default;
		EBindFlag bind_flags = EBindFlag::None;
		EBufferMiscFlag misc_flags = EBufferMiscFlag::None;
		uint32 stride = 0; //structured buffers, (vertex buffers, index buffers, needed for count calculation not for srv as structured buffers)
		EFormat format = EFormat::UNKNOWN; //typed buffers, index buffers
		std::strong_ordering operator<=>(BufferDesc const& other) const = default;
	};

	static BufferDesc VertexBufferDesc(uint64 vertex_count, uint32 stride, bool ray_tracing = true)
	{
		BufferDesc desc{};
		desc.bind_flags = ray_tracing ? EBindFlag::ShaderResource : EBindFlag::None;
		desc.resource_usage = EResourceUsage::Default;
		desc.size = vertex_count * stride;
		desc.stride = stride;
		return desc;
	}
	static BufferDesc IndexBufferDesc(uint64 index_count, bool small_indices, bool ray_tracing = true)
	{
		BufferDesc desc{};
		desc.bind_flags = ray_tracing ? EBindFlag::ShaderResource : EBindFlag::None;
		desc.resource_usage = EResourceUsage::Default;
		desc.stride = small_indices ? 2 : 4;
		desc.size = index_count * desc.stride;
		desc.format = small_indices ? EFormat::R16_UINT : EFormat::R32_UINT;
		return desc;
	}
	static BufferDesc ReadBackBufferDesc(uint64 size)
	{
		BufferDesc desc{};
		desc.bind_flags = EBindFlag::None;
		desc.resource_usage = EResourceUsage::Readback;
		desc.size = size;
		desc.misc_flags = EBufferMiscFlag::None;
		return desc;
	}
	template<typename T>
	static BufferDesc StructuredBufferDesc(uint64 count, bool uav = true, bool dynamic = false)
	{
		BufferDesc desc{};
		desc.resource_usage = (uav || !dynamic) ? EResourceUsage::Default : EResourceUsage::Upload;
		desc.bind_flags = EBindFlag::ShaderResource;
		if (uav) desc.bind_flags |= EBindFlag::UnorderedAccess;
		desc.misc_flags = EBufferMiscFlag::BufferStructured;
		desc.stride = sizeof(T);
		desc.size = desc.stride * count;
		return desc;
	}
	static BufferDesc CounterBufferDesc()
	{
		BufferDesc desc{};
		desc.size = sizeof(uint32);
		desc.bind_flags = EBindFlag::UnorderedAccess;
		return desc;
	}

	struct BufferSubresourceDesc
	{
		uint64 offset = 0;
		uint64 size = uint64(-1);
		std::strong_ordering operator<=>(BufferSubresourceDesc const& other) const = default;
	};

	using BufferInitialData = void const*;

	class Buffer
	{
	public:

		Buffer(GraphicsDevice* gfx, BufferDesc const& desc, BufferInitialData initial_data = nullptr)
			: gfx(gfx), desc(desc)
		{
			UINT64 buffer_size = desc.size;
			if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::ConstantBuffer))
				buffer_size = Align(buffer_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

			D3D12_RESOURCE_DESC resource_desc{};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.Width = buffer_size;
			resource_desc.Height = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.Alignment = 0;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
			resource_desc.SampleDesc.Count = 1;
			resource_desc.SampleDesc.Quality = 0;

			if (HasAllFlags(desc.bind_flags, EBindFlag::UnorderedAccess))
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			if (!HasAllFlags(desc.bind_flags, EBindFlag::ShaderResource))
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

			D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON;
			if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::AccelStruct))
				resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

			D3D12MA::ALLOCATION_DESC allocation_desc{};
			allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
			if (desc.resource_usage == EResourceUsage::Readback)
			{
				allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
				resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
				resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}
			else if (desc.resource_usage == EResourceUsage::Upload)
			{
				allocation_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
				resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
			}

			auto device = gfx->GetDevice();
			auto allocator = gfx->GetAllocator();
			device->GetCopyableFootprints(&resource_desc, 0, 1, 0, &footprint, nullptr, nullptr, nullptr);

			D3D12MA::Allocation* alloc = nullptr; 
			HRESULT hr = allocator->CreateResource(
				&allocation_desc,
				&resource_desc,
				resource_state,
				nullptr,
				&alloc,
				IID_PPV_ARGS(&resource)
			);
			BREAK_IF_FAILED(hr);
			allocation.reset(alloc);

			if (desc.resource_usage == EResourceUsage::Readback)
			{
				hr = resource->Map(0, nullptr, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = static_cast<uint32_t>(desc.size);
			}
			else if (desc.resource_usage == EResourceUsage::Upload)
			{
				D3D12_RANGE read_range{};
				hr = resource->Map(0, &read_range, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = static_cast<uint32>(desc.size);

				if (initial_data)
				{
					memcpy(mapped_data, initial_data, desc.size);
				}
			}

			if (initial_data != nullptr && desc.resource_usage != EResourceUsage::Upload)
			{
				auto cmd_list = gfx->GetDefaultCommandList();
				auto upload_buffer = gfx->GetDynamicAllocator();
				DynamicAllocation upload_alloc = upload_buffer->Allocate(buffer_size);
				upload_alloc.Update(initial_data, desc.size);
				cmd_list->CopyBufferRegion(
					resource.Get(),
					0,
					upload_alloc.buffer,
					upload_alloc.offset,
					desc.size);

				if (HasAnyFlag(desc.bind_flags, EBindFlag::ShaderResource))
				{
					D3D12_RESOURCE_BARRIER barrier{};
					barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					barrier.Transition.pResource = resource.Get();
					barrier.Transition.StateAfter = ConvertToD3D12ResourceState(EResourceState::NonPixelShaderResource);
					barrier.Transition.StateBefore = ConvertToD3D12ResourceState(EResourceState::CopyDest);
					barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
					cmd_list->ResourceBarrier(1, &barrier);
				}
			}
		}
		Buffer(Buffer const&) = delete;
		Buffer& operator=(Buffer const&) = delete;
		~Buffer()
		{
			if (mapped_data != nullptr)
			{
				ADRIA_ASSERT(resource != nullptr);
				resource->Unmap(0, nullptr);
				mapped_data = nullptr;
			}

			for (auto& srv : srvs) gfx->FreeOfflineDescriptor(srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			for (auto& uav : uavs) gfx->FreeOfflineDescriptor(uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(size_t i = 0) const { return GetSubresource(SubresourceType_SRV, i); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(size_t i = 0) const { return GetSubresource(SubresourceType_UAV, i); }

		[[maybe_unused]] size_t CreateSRV(BufferSubresourceDesc const* desc = nullptr)
		{
			BufferSubresourceDesc _desc = desc ? *desc : BufferSubresourceDesc{};
			return CreateSubresource(SubresourceType_SRV, _desc, nullptr);
		}
		[[maybe_unused]] size_t CreateUAV(ID3D12Resource* uav_counter = nullptr, BufferSubresourceDesc const* desc = nullptr)
		{
			BufferSubresourceDesc _desc = desc ? *desc : BufferSubresourceDesc{};
			return CreateSubresource(SubresourceType_UAV, _desc, uav_counter);
		}
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeSRV(BufferSubresourceDesc const* desc = nullptr)
		{
			BufferSubresourceDesc _desc = desc ? *desc : BufferSubresourceDesc{};
			size_t i = CreateSubresource(SubresourceType_SRV, _desc);
			ADRIA_ASSERT(srvs.size() - 1 == i);
			D3D12_CPU_DESCRIPTOR_HANDLE srv = srvs.back();
			srvs.pop_back();
			return srv;
		}
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE TakeUAV(BufferSubresourceDesc const* desc = nullptr, ID3D12Resource* uav_counter = nullptr)
		{
			BufferSubresourceDesc _desc = desc ? *desc : BufferSubresourceDesc{};
			size_t i = CreateSubresource(SubresourceType_UAV, _desc, uav_counter);
			ADRIA_ASSERT(uavs.size() - 1 == i);
			D3D12_CPU_DESCRIPTOR_HANDLE uav = uavs.back();
			uavs.pop_back();
			return uav;
		}

		ID3D12Resource* GetNative() const { return resource.Get(); }
		ID3D12Resource* Detach()  
		{ 
			return resource.Detach();
		}
		D3D12MA::Allocation* DetachAllocation()
		{
			return allocation.release();
		}
		BufferDesc const& GetDesc() const { return desc; }
		UINT GetMappedRowPitch() const { return mapped_rowpitch; }
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return resource->GetGPUVirtualAddress(); }
		UINT GetCount() const
		{
			ADRIA_ASSERT(desc.stride != 0);
			return static_cast<UINT>(desc.size / desc.stride);
		}

		bool IsMapped() const { return mapped_data != nullptr; }
		void* GetMappedData() const { return mapped_data; }
		template<typename T>
		T* GetMappedData() const { return reinterpret_cast<T*>(mapped_data); }
		[[maybe_unused]] void* Map()
		{
			if (mapped_data) return mapped_data;

			HRESULT hr;
			if (desc.resource_usage == EResourceUsage::Readback)
			{
				hr = resource->Map(0, nullptr, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = static_cast<uint32_t>(desc.size);
			}
			else if (desc.resource_usage == EResourceUsage::Upload)
			{
				D3D12_RANGE read_range{};
				hr = resource->Map(0, &read_range, &mapped_data);
				BREAK_IF_FAILED(hr);
				mapped_rowpitch = static_cast<uint32>(desc.size);
			}
			return mapped_data;
		}
		void Unmap()
		{
			resource->Unmap(0, nullptr);
			mapped_data = nullptr;
			mapped_rowpitch = 0;
		}
		void Update(void const* src_data, size_t data_size)
		{
			ADRIA_ASSERT(desc.resource_usage == EResourceUsage::Upload);
			if (mapped_data)
			{
				memcpy(mapped_data, src_data, data_size);
			}
			else
			{
				Map();
				ADRIA_ASSERT(mapped_data);
				memcpy(mapped_data, src_data, data_size);
			}
		}
		template<typename T>
		void Update(T const& src_data)
		{
			Update(&src_data, sizeof(T));
		}
		void SetName(char const* name)
		{
			resource->SetName(ToWideString(name).c_str());
		}
	private:
		GraphicsDevice* gfx;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		BufferDesc desc;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srvs;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> uavs;

		ReleasablePtr<D3D12MA::Allocation> allocation = nullptr;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;

		void* mapped_data = nullptr;
		uint32 mapped_rowpitch = 0;

	private:

		size_t CreateSubresource(ESubresourceType view_type, BufferSubresourceDesc const& view_desc,
			ID3D12Resource* uav_counter = nullptr)
		{
			if (uav_counter) ADRIA_ASSERT(view_type == SubresourceType_UAV);
			EFormat format = desc.format;
			D3D12_CPU_DESCRIPTOR_HANDLE heap_descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			switch (view_type)
			{
			case SubresourceType_SRV:
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
				srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				bool is_accel_struct = false;
				if (format == EFormat::UNKNOWN)
				{
					if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::BufferRaw))
					{
						// This is a Raw Buffer
						srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
						srv_desc.Buffer.FirstElement = (UINT)view_desc.offset / sizeof(uint32_t);
						srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
						srv_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32_t);
					}
					else if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::BufferStructured))
					{
						// This is a Structured Buffer
						srv_desc.Format = DXGI_FORMAT_UNKNOWN;
						srv_desc.Buffer.FirstElement = (UINT)view_desc.offset / desc.stride;
						srv_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
						srv_desc.Buffer.StructureByteStride = desc.stride;
					}
					else if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::AccelStruct))
					{
						is_accel_struct = true;
						srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
						srv_desc.RaytracingAccelerationStructure.Location = GetGPUAddress();
					}
				}
				else
				{
					// This is a Typed Buffer
					uint32_t stride = GetFormatStride(format);
					srv_desc.Format = ConvertFormat(format);
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
					srv_desc.Buffer.FirstElement = view_desc.offset / stride;
					srv_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / stride;
				}

				Microsoft::WRL::ComPtr<ID3D12Device> device;
				resource->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

				device->CreateShaderResourceView(!is_accel_struct ? resource.Get() : nullptr, &srv_desc, heap_descriptor);
				srvs.push_back(heap_descriptor);
				return srvs.size() - 1;
			}
			break;
			case SubresourceType_UAV:
			{
				D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uav_desc.Buffer.FirstElement = 0;

				if (format == EFormat::UNKNOWN)
				{
					if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::BufferRaw))
					{
						uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
						uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
						uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / sizeof(uint32_t);
						uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32_t);
					}
					else if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::BufferStructured))
					{
						uav_desc.Format = DXGI_FORMAT_UNKNOWN;
						uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / desc.stride;
						uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
						uav_desc.Buffer.StructureByteStride = desc.stride;
					}
					else if (HasAllFlags(desc.misc_flags, EBufferMiscFlag::IndirectArgs))
					{
						uav_desc.Format = DXGI_FORMAT_R32_UINT;
						uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / sizeof(uint32_t);
						uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32_t);

					}
				}
				else
				{
					uint32 stride = GetFormatStride(format);
					uav_desc.Format = ConvertFormat(format);
					uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / stride;
					uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / stride;
				}

				Microsoft::WRL::ComPtr<ID3D12Device> device;
				resource->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

				device->CreateUnorderedAccessView(resource.Get(), uav_counter, &uav_desc, heap_descriptor);
				uavs.push_back(heap_descriptor);
				return uavs.size() - 1;
			}
			break;
			case SubresourceType_RTV:
			case SubresourceType_DSV:
			default:
				ADRIA_ASSERT(false && "Buffer View can only be UAV or SRV!");
			}
			return -1;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetSubresource(ESubresourceType type, size_t index = 0) const
		{
			switch (type)
			{
			case SubresourceType_SRV:
				ADRIA_ASSERT(index < srvs.size());
				return srvs[index];
			case SubresourceType_UAV:
				ADRIA_ASSERT(index < uavs.size());
				return uavs[index];
			case SubresourceType_RTV:
			case SubresourceType_DSV:
			default:
				ADRIA_ASSERT(false && "Invalid view type for buffer!");
			}
			return { .ptr = NULL };
		}
	};

	inline void BindVertexBuffer(ID3D12GraphicsCommandList* cmd_list, Buffer const* vertex_buffer)
	{
		D3D12_VERTEX_BUFFER_VIEW vb_view{};
		vb_view.BufferLocation = vertex_buffer->GetGPUAddress();
		vb_view.SizeInBytes = (UINT)vertex_buffer->GetDesc().size;
		vb_view.StrideInBytes = vertex_buffer->GetDesc().stride;
		cmd_list->IASetVertexBuffers(0, 1, &vb_view);
	}
	inline void BindIndexBuffer(ID3D12GraphicsCommandList* cmd_list, Buffer const* index_buffer)
	{
		D3D12_INDEX_BUFFER_VIEW ib_view{};
		ib_view.BufferLocation = index_buffer->GetGPUAddress();
		ib_view.Format = ConvertFormat(index_buffer->GetDesc().format);
		ib_view.SizeInBytes = (UINT)index_buffer->GetDesc().size;
		cmd_list->IASetIndexBuffer(&ib_view);
	}

}