#pragma once
#include <string>
#include "../Graphics/Texture.h"
#include "RenderGraphResourceHandle.h"

namespace adria
{
	class RenderGraphPassBase;
	class RenderGraph;

	struct RenderGraphTexture
	{
		RenderGraphTexture(char const* name, size_t id, Texture* texture)
			: name(name), id(id), imported(true), version(0), texture(texture),
			  desc(texture->GetDesc()), ref_count(0)
		{}

		RenderGraphTexture(char const* name, size_t id, TextureDesc const& desc)
			: name(name), id(id), imported(false), version(0), texture(nullptr),
			  desc(desc), ref_count(0)
		{}

		std::string name;
		size_t id;
		bool imported;
		size_t version;
		Texture* texture;
		TextureDesc desc;
		size_t ref_count;
	};

	using RGTexture = RenderGraphTexture;

	struct RenderGraphResourceNode
	{
		explicit RenderGraphResourceNode(RGTexture* resource)
			: resource(resource), version(resource->version)
		{}
		RGTexture* resource;
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

		RGTexture& GetResource(RGResourceHandle handle);

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