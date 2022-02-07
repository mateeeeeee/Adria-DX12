#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "ResourceBarrierBatch.h"

namespace adria
{

	struct texture2d_srv_desc_t
	{
		DXGI_FORMAT format;
	};

	struct texture2d_dsv_desc_t
	{
		DXGI_FORMAT format;
		D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE;
	};

	struct texture2d_desc_t
	{
		uint32 width;
		uint32 height;
		DXGI_FORMAT format;
		D3D12_CLEAR_VALUE clear_value;
		uint32 mips = 1;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_RESOURCE_STATES start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	};

	class Texture2D
	{
	public:
		Texture2D() = default;

		Texture2D(ID3D12Device* device, texture2d_desc_t const& desc) : device(device)
		{
			D3D12_RESOURCE_DESC hdr_desc{};
			hdr_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			hdr_desc.DepthOrArraySize = 1;
			hdr_desc.MipLevels = desc.mips;
			hdr_desc.Flags = desc.flags;
			hdr_desc.Format = desc.format;
			hdr_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			hdr_desc.SampleDesc.Count = 1u;
			hdr_desc.SampleDesc.Quality = 0u;
			hdr_desc.Width = desc.width;
			hdr_desc.Height = desc.height;
			auto heap_type = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			D3D12_CLEAR_VALUE const* clear = nullptr;

			if (desc.flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
				clear = &desc.clear_value;

			BREAK_IF_FAILED(device->CreateCommittedResource(
				&heap_type,
				D3D12_HEAP_FLAG_NONE,
				&hdr_desc,
				desc.start_state,
				clear,
				IID_PPV_ARGS(&resource)
			));
		}

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE handle, texture2d_srv_desc_t* _srv_desc = nullptr)
		{
			auto desc = resource->GetDesc();

			ADRIA_ASSERT(!(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Format = _srv_desc ? _srv_desc->format : desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Texture2D.MostDetailedMip = 0;
			srv_desc.Texture2D.MipLevels = -1;
			srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

			device->CreateShaderResourceView(resource.Get(), &srv_desc, handle);

			srv_handle = handle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE SRV() const
		{
			return srv_handle;
		}

		void CreateRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			auto desc = resource->GetDesc();
			ADRIA_ASSERT(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtv_desc.Format = desc.Format;
			rtv_desc.Texture2D.MipSlice = 0;
			rtv_desc.Texture2D.PlaneSlice = 0;

			device->CreateRenderTargetView(resource.Get(), &rtv_desc, handle);

			rtv_handle = handle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE RTV() const
		{
			return rtv_handle;
		}

		void CreateUAV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uav_desc.Format = resource->GetDesc().Format;
			uav_desc.Texture2D.MipSlice = 0;
			uav_desc.Texture2D.PlaneSlice = 0;

			device->CreateUnorderedAccessView(resource.Get(), nullptr, &uav_desc, handle);

			uav_handle = handle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE UAV() const
		{
			return uav_handle;
		}

		void CreateDSV(D3D12_CPU_DESCRIPTOR_HANDLE handle, texture2d_dsv_desc_t* _dsv_desc = nullptr)
		{
			auto desc = resource->GetDesc();

			ADRIA_ASSERT(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
			dsv_desc.Texture2D.MipSlice = 0;
			dsv_desc.Format = _dsv_desc ? _dsv_desc->format :  desc.Format;
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv_desc.Flags = _dsv_desc ? _dsv_desc->flags : D3D12_DSV_FLAG_NONE;

			device->CreateDepthStencilView(resource.Get(), &dsv_desc, handle);

			dsv_handle = handle;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE DSV() const
		{
			return dsv_handle;
		}

		void Transition(ResourceBarrierBatch& barriers, D3D12_RESOURCE_STATES state_before,
			D3D12_RESOURCE_STATES state_after) const
		{
			barriers.AddTransition(resource.Get(), state_before, state_after);
		}

		ID3D12Resource* Resource() const
		{
			return resource.Get();
		}

		uint32 Width() const
		{
			return (uint32)resource->GetDesc().Width;
		}

		uint32 Height() const
		{
			return (uint32)resource->GetDesc().Height;
		}

		DXGI_FORMAT Format() const
		{
			return resource->GetDesc().Format;
		}

	private:
		ID3D12Device* device;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{};
		D3D12_CPU_DESCRIPTOR_HANDLE srv_handle{};
		D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle{};
		D3D12_CPU_DESCRIPTOR_HANDLE uav_handle{};
		
	};
}