#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include "d3dx12.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "ResourceBarrierBatch.h"

namespace adria
{


	struct texturecube_desc_t
	{
		u32 width;
		u32 height;
		u32 levels = 1u;
		DXGI_FORMAT format = DXGI_FORMAT_R32_TYPELESS;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_RESOURCE_STATES start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		D3D12_CLEAR_VALUE clear_value;
	};

	struct texturecube_srv_desc_t
	{
		DXGI_FORMAT format;
	};

	struct texturecube_rtv_desc_t
	{

	};

	struct texturecube_dsv_desc_t
	{
		DXGI_FORMAT format;
	};

	class TextureCube
	{
	public:
		TextureCube() = default;

		TextureCube(ID3D12Device* device, texturecube_desc_t const& desc) : device(device)
		{
			D3D12_RESOURCE_DESC texture_desc = {};
			texture_desc.Width = desc.width;
			texture_desc.Height = desc.height;
			texture_desc.MipLevels = desc.levels > 0 ? desc.levels : 0;
			texture_desc.DepthOrArraySize = 6;
			texture_desc.Format = desc.format;
			texture_desc.SampleDesc.Count = 1;
			texture_desc.SampleDesc.Quality = 0;
			texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texture_desc.Flags = desc.flags;

			D3D12_CLEAR_VALUE clearValue{};
			clearValue.Format = DXGI_FORMAT_D32_FLOAT;
			clearValue.DepthStencil.Depth = 1.0f;
			clearValue.DepthStencil.Stencil = 0;

			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, desc.start_state, &desc.clear_value,
				IID_PPV_ARGS(&resource));
		}

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE handle, texturecube_srv_desc_t* _srv_desc = nullptr)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Format = _srv_desc ? _srv_desc->format : resource->GetDesc().Format; 
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srv_desc.TextureCube.MipLevels = 1;
			srv_desc.TextureCube.MostDetailedMip = 0;
			srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;

			device->CreateShaderResourceView(resource.Get(), &srv_desc, handle);

			srv_handle = handle;
		}

		auto SRV() const
		{
			return srv_handle;
		}

		void CreateDSVs(std::array<D3D12_CPU_DESCRIPTOR_HANDLE,6> const& handles, texturecube_dsv_desc_t* _dsv_desc = nullptr)
		{

			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};

			dsv_desc.Format = _dsv_desc ? _dsv_desc->format : resource->GetDesc().Format;
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsv_desc.Texture2DArray.MipSlice = 0;
			dsv_desc.Texture2DArray.ArraySize = 1;
			dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

			for (u32 face = 0; face < 6; ++face)
			{
				dsv_desc.Texture2DArray.FirstArraySlice = face;
	
				device->CreateDepthStencilView(resource.Get(), &dsv_desc, handles[face]);
			}

			dsv_handles = handles;
		}

		auto DSV(u32 i) const
		{
			return dsv_handles[i];
		}

		void CreateRTVs(std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6> const& handles, texturecube_rtv_desc_t* _rtv_desc = nullptr)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
			rtv_desc.Format = resource->GetDesc().Format;
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv_desc.Texture2DArray.MipSlice = 0;
			rtv_desc.Texture2DArray.ArraySize = 1;
			rtv_desc.Texture2DArray.PlaneSlice = 0;


			for (u32 face = 0; face < 6; ++face)
			{
				rtv_desc.Texture2DArray.FirstArraySlice = face;
				device->CreateRenderTargetView(resource.Get(), &rtv_desc, handles[face]);
			}

			rtv_handles = handles;
		}

		auto RTV(u32 i) const
		{
			return rtv_handles[i];
		}


		void Transition(ResourceBarrierBatch& barriers, D3D12_RESOURCE_STATES state_before,
			D3D12_RESOURCE_STATES state_after)
		{
			barriers.AddTransition(resource.Get(), state_before, state_after);
		}

		
	private:
		ID3D12Device* device;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
		std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6> dsv_handles;
		std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6> rtv_handles;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_handle{};
		D3D12_CPU_DESCRIPTOR_HANDLE uav_handle{};
	};

}