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

	struct RenderGraphTextureNode
	{
		explicit RenderGraphTextureNode(RGTexture* resource)
			: resource(resource), version(resource->version)
		{}
		RGTexture* resource;
		size_t version;
		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
	};
	using RGTextureNode = RenderGraphTextureNode;


	using RGResourceView = D3D12_CPU_DESCRIPTOR_HANDLE;
	enum class RGResourceViewType
	{
		SRV,
		UAV,
		RTV,
		DSV
	};

	class RenderGraphResources
	{
		friend RenderGraph;


	public:
		RenderGraphResources() = delete;
		RenderGraphResources(RenderGraphResources const&) = delete;
		RenderGraphResources& operator=(RenderGraphResources const&) = delete;

		RGTexture& GetTexture(RGResourceHandle handle);

		template<RGResourceViewType ViewType>
		RGResourceView CreateTextureView(RGResourceHandle handle, TextureViewDesc const& view_desc)
		{
			if constexpr (ViewType == RGResourceViewType::SRV)
				return CreateShaderResourceView(handle, view_desc);
			else if constexpr (ViewType == RGResourceViewType::UAV)
				return CreateUnorderedAccessView(handle, view_desc);
			else if constexpr (ViewType == RGResourceViewType::RTV)
				return CreateRenderTargetView(handle, view_desc);
			else if constexpr (ViewType == RGResourceViewType::DSV)
				return CreateDepthStencilView(handle, view_desc);
			else  RGResourceView{ .ptr = NULL };
		}

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass);

		RGResourceView CreateShaderResourceView(RGResourceHandle handle, TextureViewDesc  const& desc);
		RGResourceView CreateRenderTargetView(RGResourceHandle handle, TextureViewDesc    const& desc);
		RGResourceView CreateUnorderedAccessView(RGResourceHandle handle, TextureViewDesc const& desc);
		RGResourceView CreateDepthStencilView(RGResourceHandle handle, TextureViewDesc    const& desc);

	};
}