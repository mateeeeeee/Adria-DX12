#pragma once
#include <d3d12.h>

namespace adria
{
	struct RendererGlobalData
	{
		D3D12_GPU_VIRTUAL_ADDRESS   frame_cbuffer_address;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE null_uav_texture2d;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texturecube;
		D3D12_CPU_DESCRIPTOR_HANDLE null_srv_texture2darray;
	};
}