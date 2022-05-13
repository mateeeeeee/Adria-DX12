#pragma once
#include <string>
#include <d3d12.h>
#include <wrl.h>
#include "RenderGraphResourceHandle.h"

namespace adria
{
	class RenderGraphPassBase;
	class RenderGraph;

	struct RenderGraphResource
	{
		RenderGraphResource(char const* name, size_t id, ID3D12Resource* resource)
			: name(name), id(id), imported(true), version(0), resource(resource), 
			  desc(resource->GetDesc()), ref_count(0)
		{}

		RenderGraphResource(char const* name, size_t id, D3D12_RESOURCE_DESC const& desc)
			: name(name), id(id), imported(false), version(0), resource(nullptr), 
			  desc(desc), ref_count(0)
		{}

		void SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE _srv)
		{
			srv = _srv;
		}
		void SetUAV(D3D12_CPU_DESCRIPTOR_HANDLE _uav)
		{
			uav = _uav;
		}
		void SetRTV(D3D12_CPU_DESCRIPTOR_HANDLE _rtv)
		{
			rtv = _rtv;
		}
		void SetDSV(D3D12_CPU_DESCRIPTOR_HANDLE _dsv)
		{
			dsv = _dsv;
		}

		std::string name;
		size_t id;
		bool imported;
		size_t version;
		ID3D12Resource* resource;
		D3D12_RESOURCE_DESC desc;
		D3D12_CPU_DESCRIPTOR_HANDLE srv = { NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE uav = { NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = { NULL };
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = { NULL };
		size_t ref_count;
	};

	using RGResource = RenderGraphResource;

	struct RenderGraphResourceNode
	{
		explicit RenderGraphResourceNode(RGResource* resource)
			: resource(resource), version(resource->version)
		{}
		RGResource* resource;
		size_t version;
		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
	};

	using RGResourceNode = RenderGraphResourceNode;

	class RenderGraphResources
	{
		friend RenderGraph;
	public:
		RenderGraphResources() = delete;
		RenderGraphResources(RenderGraphResources const&) = delete;
		RenderGraphResources& operator=(RenderGraphResources const&) = delete;

		RGResource& GetResource(RGResourceHandle handle);

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass);
	};
}