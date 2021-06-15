#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "d3dx12.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "ResourceBarriers.h"

namespace adria
{
	
	struct texture2darray_desc_t
	{
		u32 width;
		u32 height;
		u32 array_size;
		DXGI_FORMAT format = DXGI_FORMAT_R32_TYPELESS;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_RESOURCE_STATES start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		D3D12_CLEAR_VALUE clear_value;
	};

	struct texture2darray_srv_desc_t
	{
		DXGI_FORMAT format;
	};

	struct texture2darray_rtv_desc_t
	{

	};

	struct texture2darray_dsv_desc_t
	{
		DXGI_FORMAT format;
	};


	class Texture2DArray
	{
	public:
		Texture2DArray() = default;

		Texture2DArray(ID3D12Device* device, texture2darray_desc_t const& desc) : device(device)
		{
			D3D12_RESOURCE_DESC texture_desc = {};
			texture_desc.Alignment = 0;
			texture_desc.MipLevels = 1;
			texture_desc.Format = desc.format;
			texture_desc.Width = desc.width;
			texture_desc.Height = desc.height;
			texture_desc.Flags = desc.flags;
			texture_desc.DepthOrArraySize = desc.array_size;
			texture_desc.SampleDesc.Count = 1;
			texture_desc.SampleDesc.Quality = 0;
			texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;


			auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &desc.clear_value,
				IID_PPV_ARGS(&resource));

		}

		void CreateSRV(D3D12_CPU_DESCRIPTOR_HANDLE handle, texture2darray_srv_desc_t* _srv_desc = nullptr)
		{

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.Format = _srv_desc ? _srv_desc->format : resource->GetDesc().Format;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv_desc.Texture2DArray.MipLevels = 1;
			srv_desc.Texture2DArray.MostDetailedMip = 0;
			srv_desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
			srv_desc.Texture2DArray.PlaneSlice = 0;
			srv_desc.Texture2DArray.ArraySize = resource->GetDesc().DepthOrArraySize;

			device->CreateShaderResourceView(resource.Get(), &srv_desc, handle);

			srv_handle = handle;
		}

		auto SRV() const
		{
			return srv_handle;
		}

		void CreateDSVs(std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> const& handles, texture2darray_dsv_desc_t* _dsv_desc = nullptr)
		{
			ADRIA_ASSERT(handles.size() == resource->GetDesc().DepthOrArraySize);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};

			dsv_desc.Format = _dsv_desc ? _dsv_desc->format : resource->GetDesc().Format;
			dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsv_desc.Texture2DArray.MipSlice = 0;
			dsv_desc.Texture2DArray.ArraySize = 1;
			dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

			for (u32 i = 0; i < handles.size(); ++i)
			{
				dsv_desc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);
				device->CreateDepthStencilView(resource.Get(), &dsv_desc, handles[i]);
			}

			dsv_handles = handles;
		}

		auto DSV(u32 i) const
		{
			return dsv_handles[i];
		}

		void CreateRTVs(std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> const& handles, texture2darray_rtv_desc_t* _rtv_desc = nullptr)
		{
			ADRIA_ASSERT(handles.size() == resource->GetDesc().DepthOrArraySize);

			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
			rtv_desc.Format = resource->GetDesc().Format;
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv_desc.Texture2DArray.MipSlice = 0;
			rtv_desc.Texture2DArray.ArraySize = 1;
			rtv_desc.Texture2DArray.PlaneSlice = 0;

			for (u32 i = 0; i < handles.size(); ++i)
			{
				rtv_desc.Texture2DArray.FirstArraySlice = D3D12CalcSubresource(0, i, 0, 1, 1);;
				device->CreateRenderTargetView(resource.Get(), &rtv_desc, handles[i]);
			}

			rtv_handles = handles;
		}

		auto RTV(u32 i) const
		{
			return rtv_handles[i];
		}




		void Transition(ResourceBarriers& barriers, D3D12_RESOURCE_STATES state_before,
			D3D12_RESOURCE_STATES state_after)
		{
			barriers.AddTransition(resource.Get(), state_before, state_after);
		}


	private:
		ID3D12Device* device;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_handle;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtv_handles;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dsv_handles;

	};

}