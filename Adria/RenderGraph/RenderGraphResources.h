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

		std::string name;
		size_t id;
		bool imported;
		size_t version;
		ID3D12Resource* resource;
		D3D12_RESOURCE_DESC desc;
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

	using RGResourceView = D3D12_CPU_DESCRIPTOR_HANDLE;
	class RenderGraphResources
	{
		friend RenderGraph;
	public:
		RenderGraphResources() = delete;
		RenderGraphResources(RenderGraphResources const&) = delete;
		RenderGraphResources& operator=(RenderGraphResources const&) = delete;

		RGResource& GetResource(RGResourceHandle handle);

		template<typename DescType>
		RGResourceView CreateResourceView(RGResourceHandle handle, DescType&& desc)
		{
			if constexpr (std::is_same_v<std::remove_cvref_t<DescType>, D3D12_SHADER_RESOURCE_VIEW_DESC>)
			{
				return CreateShaderResourceView(handle, std::forward<DescType>(desc));
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<DescType>, D3D12_RENDER_TARGET_VIEW_DESC>)
			{
				return CreateRenderTargetView(handle, std::forward<DescType>(desc));
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<DescType>, D3D12_UNORDERED_ACCESS_VIEW_DESC>)
			{
				return CreateUnorderedAccessView(handle, std::forward<DescType>(desc));
			}
			else if constexpr (std::is_same_v<std::remove_cvref_t<DescType>, D3D12_DEPTH_STENCIL_VIEW_DESC>)
			{
				return CreateDepthStencilView(handle, std::forward<DescType>(desc));
			}
			return RGResourceView{ .ptr = NULL };
		}

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass);

		RGResourceView CreateShaderResourceView(RGResourceHandle handle, D3D12_SHADER_RESOURCE_VIEW_DESC  const& desc);
		RGResourceView CreateRenderTargetView(RGResourceHandle handle, D3D12_RENDER_TARGET_VIEW_DESC    const& desc);
		RGResourceView CreateUnorderedAccessView(RGResourceHandle handle, D3D12_UNORDERED_ACCESS_VIEW_DESC const& desc);
		RGResourceView CreateDepthStencilView(RGResourceHandle handle, D3D12_DEPTH_STENCIL_VIEW_DESC    const& desc);

	};
}