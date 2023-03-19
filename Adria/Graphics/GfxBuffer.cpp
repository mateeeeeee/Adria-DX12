#include "GfxBuffer.h"
#include "LinearDynamicAllocator.h"

#include <format>

namespace adria
{

	GfxBuffer::GfxBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferInitialData initial_data /*= nullptr*/) : gfx(gfx), desc(desc)
	{
		UINT64 buffer_size = desc.size;
		if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::ConstantBuffer))
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

		if (HasAllFlags(desc.bind_flags, GfxBindFlag::UnorderedAccess))
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		if (!HasAllFlags(desc.bind_flags, GfxBindFlag::ShaderResource))
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::AccelStruct))
			resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		D3D12MA::ALLOCATION_DESC allocation_desc{};
		allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
			resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
			resource_desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
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
			IID_PPV_ARGS(resource.GetAddressOf())
		);
		BREAK_IF_FAILED(hr);
		allocation.reset(alloc);

		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			BREAK_IF_FAILED(hr);
			mapped_rowpitch = static_cast<uint32_t>(desc.size);
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
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

		if (initial_data != nullptr && desc.resource_usage != GfxResourceUsage::Upload)
		{
			auto cmd_list = gfx->GetCommandList();
			auto upload_buffer = gfx->GetDynamicAllocator();
			DynamicAllocation upload_alloc = upload_buffer->Allocate(buffer_size);
			upload_alloc.Update(initial_data, desc.size);
			cmd_list->CopyBufferRegion(
				resource.Get(),
				0,
				upload_alloc.buffer,
				upload_alloc.offset,
				desc.size);

			if (HasAnyFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = resource.Get();
				barrier.Transition.StateAfter = ConvertToD3D12ResourceState(GfxResourceState::NonPixelShaderResource);
				barrier.Transition.StateBefore = ConvertToD3D12ResourceState(GfxResourceState::CopyDest);
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				cmd_list->ResourceBarrier(1, &barrier);
			}
		}
	}

	GfxBuffer::~GfxBuffer()
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

	D3D12_CPU_DESCRIPTOR_HANDLE GfxBuffer::GetSRV(size_t i /*= 0*/) const
	{
		return GetSubresource(GfxSubresourceType::SRV, i);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxBuffer::GetUAV(size_t i /*= 0*/) const
	{
		return GetSubresource(GfxSubresourceType::UAV, i);
	}

	size_t GfxBuffer::CreateSRV(GfxBufferSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxBufferSubresourceDesc _desc = desc ? *desc : GfxBufferSubresourceDesc{};
		return CreateSubresource(GfxSubresourceType::SRV, _desc, nullptr);
	}

	size_t GfxBuffer::CreateUAV(ID3D12Resource* uav_counter /*= nullptr*/, GfxBufferSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxBufferSubresourceDesc _desc = desc ? *desc : GfxBufferSubresourceDesc{};
		return CreateSubresource(GfxSubresourceType::UAV, _desc, uav_counter);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxBuffer::TakeSRV(GfxBufferSubresourceDesc const* desc /*= nullptr*/)
	{
		GfxBufferSubresourceDesc _desc = desc ? *desc : GfxBufferSubresourceDesc{};
		size_t i = CreateSubresource(GfxSubresourceType::SRV, _desc);
		ADRIA_ASSERT(srvs.size() - 1 == i);
		D3D12_CPU_DESCRIPTOR_HANDLE srv = srvs.back();
		srvs.pop_back();
		return srv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxBuffer::TakeUAV(GfxBufferSubresourceDesc const* desc /*= nullptr*/, ID3D12Resource* uav_counter /*= nullptr*/)
	{
		GfxBufferSubresourceDesc _desc = desc ? *desc : GfxBufferSubresourceDesc{};
		size_t i = CreateSubresource(GfxSubresourceType::UAV, _desc, uav_counter);
		ADRIA_ASSERT(uavs.size() - 1 == i);
		D3D12_CPU_DESCRIPTOR_HANDLE uav = uavs.back();
		uavs.pop_back();
		return uav;
	}

	void* GfxBuffer::GetMappedData() const
	{
		return mapped_data;
	}

	ID3D12Resource* GfxBuffer::GetNative() const
	{
		return resource.Get();
	}

	ID3D12Resource* GfxBuffer::Detach()
	{

		return resource.Detach();
	}

	D3D12MA::Allocation* GfxBuffer::DetachAllocation()
	{
		return allocation.release();
	}

	GfxBufferDesc const& GfxBuffer::GetDesc() const
	{
		return desc;
	}

	uint32 GfxBuffer::GetMappedRowPitch() const
	{
		return mapped_rowpitch;
	}

	uint64 GfxBuffer::GetGPUAddress() const
	{
		return resource->GetGPUVirtualAddress();
	}

	uint32 GfxBuffer::GetCount() const
	{
		ADRIA_ASSERT(desc.stride != 0);
		return static_cast<UINT>(desc.size / desc.stride);
	}

	bool GfxBuffer::IsMapped() const
	{
		return mapped_data != nullptr;
	}

	void* GfxBuffer::Map()
	{
		if (mapped_data) return mapped_data;

		HRESULT hr;
		if (desc.resource_usage == GfxResourceUsage::Readback)
		{
			hr = resource->Map(0, nullptr, &mapped_data);
			BREAK_IF_FAILED(hr);
			mapped_rowpitch = static_cast<uint32_t>(desc.size);
		}
		else if (desc.resource_usage == GfxResourceUsage::Upload)
		{
			D3D12_RANGE read_range{};
			hr = resource->Map(0, &read_range, &mapped_data);
			BREAK_IF_FAILED(hr);
			mapped_rowpitch = static_cast<uint32>(desc.size);
		}
		return mapped_data;
	}

	void GfxBuffer::Unmap()
	{
		resource->Unmap(0, nullptr);
		mapped_data = nullptr;
		mapped_rowpitch = 0;
	}

	void GfxBuffer::Update(void const* src_data, size_t data_size, size_t offset /*= 0*/)
	{
		ADRIA_ASSERT(desc.resource_usage == GfxResourceUsage::Upload);
		if (mapped_data)
		{
			memcpy((uint8*)mapped_data + offset, src_data, data_size);
		}
		else
		{
			Map();
			ADRIA_ASSERT(mapped_data);
			memcpy((uint8*)mapped_data + offset, src_data, data_size);
		}
	}

	void GfxBuffer::SetName(char const* name)
	{
		resource->SetName(ToWideString(name).c_str());
	}

	size_t GfxBuffer::CreateSubresource(GfxSubresourceType view_type, GfxBufferSubresourceDesc const& view_desc, ID3D12Resource* uav_counter /*= nullptr*/)
	{
		if (uav_counter) ADRIA_ASSERT(view_type == GfxSubresourceType::UAV);
		GfxFormat format = desc.format;
		D3D12_CPU_DESCRIPTOR_HANDLE heap_descriptor = gfx->AllocateOfflineDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		switch (view_type)
		{
		case GfxSubresourceType::SRV:
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			bool is_accel_struct = false;
			if (format == GfxFormat::UNKNOWN)
			{
				if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferRaw))
				{
					// This is a Raw Buffer
					srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					srv_desc.Buffer.FirstElement = (UINT)view_desc.offset / sizeof(uint32_t);
					srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
					srv_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32_t);
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferStructured))
				{
					// This is a Structured Buffer
					srv_desc.Format = DXGI_FORMAT_UNKNOWN;
					srv_desc.Buffer.FirstElement = (UINT)view_desc.offset / desc.stride;
					srv_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
					srv_desc.Buffer.StructureByteStride = desc.stride;
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::AccelStruct))
				{
					is_accel_struct = true;
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
					srv_desc.RaytracingAccelerationStructure.Location = GetGPUAddress();
				}
			}
			else
			{
				// This is a Typed Buffer
				uint32_t stride = GetGfxFormatStride(format);
				srv_desc.Format = ConvertGfxFormat(format);
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srv_desc.Buffer.FirstElement = view_desc.offset / stride;
				srv_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / stride;
			}
			gfx->GetDevice()->CreateShaderResourceView(!is_accel_struct ? resource.Get() : nullptr, &srv_desc, heap_descriptor);
			srvs.push_back(heap_descriptor);
			return srvs.size() - 1;
		}
		break;
		case GfxSubresourceType::UAV:
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;

			if (format == GfxFormat::UNKNOWN)
			{
				if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferRaw))
				{
					uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
					uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / sizeof(uint32_t);
					uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32_t);
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferStructured))
				{
					uav_desc.Format = DXGI_FORMAT_UNKNOWN;
					uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / desc.stride;
					uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
					uav_desc.Buffer.StructureByteStride = desc.stride;
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::IndirectArgs))
				{
					uav_desc.Format = DXGI_FORMAT_R32_UINT;
					uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / sizeof(uint32_t);
					uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / sizeof(uint32_t);

				}
			}
			else
			{
				uint32 stride = GetGfxFormatStride(format);
				uav_desc.Format = ConvertGfxFormat(format);
				uav_desc.Buffer.FirstElement = (UINT)view_desc.offset / stride;
				uav_desc.Buffer.NumElements = (UINT)std::min<UINT64>(view_desc.size, desc.size - view_desc.offset) / stride;
			}

			gfx->GetDevice()->CreateUnorderedAccessView(resource.Get(), uav_counter, &uav_desc, heap_descriptor);
			uavs.push_back(heap_descriptor);
			return uavs.size() - 1;
		}
		break;
		case GfxSubresourceType::RTV:
		case GfxSubresourceType::DSV:
		default:
			ADRIA_ASSERT(false && "Buffer View can only be UAV or SRV!");
		}
		return -1;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GfxBuffer::GetSubresource(GfxSubresourceType type, size_t index /*= 0*/) const
	{
		switch (type)
		{
		case GfxSubresourceType::SRV:
			ADRIA_ASSERT(index < srvs.size());
			return srvs[index];
		case GfxSubresourceType::UAV:
			ADRIA_ASSERT(index < uavs.size());
			return uavs[index];
		case GfxSubresourceType::RTV:
		case GfxSubresourceType::DSV:
		default:
			ADRIA_ASSERT(false && "Invalid view type for buffer!");
		}
		return { .ptr = NULL };
	}

}