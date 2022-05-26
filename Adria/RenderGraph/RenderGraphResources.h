#pragma once
#include <string>
#include "RenderGraphResourceHandle.h"

namespace adria
{
	class RenderGraphPassBase;
	class RenderGraph;

	template<typename ResourceType>
	struct RGResourceTraits;

	template<>
	struct RGResourceTraits<Texture>
	{
		using ResourceDesc = TextureDesc;
	};

	template<>
	struct RGResourceTraits<Buffer>
	{
		using ResourceDesc = BufferDesc;
	};

	struct RenderGraphResource
	{
		RenderGraphResource(char const* name, size_t id, bool imported)
			: name(name), id(id), imported(imported), version(0), ref_count(0)
		{}

		std::string name;
		size_t id;
		bool imported;
		size_t version;
		size_t ref_count;
	};

	template<typename ResourceType>
	struct TypedRenderGraphResource : RenderGraphResource
	{
		using ResourceDesc = RGResourceTraits<ResourceType>::ResourceDesc;

		TypedRenderGraphResource(char const* name, size_t id, ResourceType* resource)
			: RenderGraphResource(name, id, true), resource(resource), desc(resource->GetDesc())
		{}

		TypedRenderGraphResource(char const* name, size_t id, ResourceDesc const& desc)
			: RenderGraphResource(name, id, true), resource(nullptr), desc(desc)
		{}

		ResourceType* resource;
		ResourceDesc desc;
	};
	using RGTexture = TypedRenderGraphResource<Texture>;
	using RGBuffer = TypedRenderGraphResource<Buffer>;

	struct RenderGraphTextureNode
	{
		explicit RenderGraphTextureNode(RGTexture* texture)
			: texture(texture), version(texture->version)
		{}
		RGTexture* texture;
		size_t version;
		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
	};
	using RGTextureNode = RenderGraphTextureNode;

	struct RenderGraphBufferNode
	{
		explicit RenderGraphBufferNode(RGBuffer* buffer)
			: buffer(buffer), version(buffer->version)
		{}
		RGBuffer* buffer;
		size_t version;
		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
	};
	using RGBufferNode = RenderGraphBufferNode;

	using RGResourceView = D3D12_CPU_DESCRIPTOR_HANDLE;
	enum RGResourceViewType
	{
		RG_SRV,
		RG_UAV,
		RG_RTV,
		RG_DSV
	};

	class RenderGraphResources
	{
		friend RenderGraph;
	public:
		RenderGraphResources() = delete;
		RenderGraphResources(RenderGraphResources const&) = delete;
		RenderGraphResources& operator=(RenderGraphResources const&) = delete;

		Texture& GetTexture(RGTextureHandle handle);
		Buffer& GetBuffer(RGBufferHandle handle);

		template<RGResourceViewType ViewType>
		RGResourceView CreateTextureView(RGTextureHandle handle, TextureViewDesc const& view_desc)
		{
			switch (ViewType)
			{
			case RG_SRV:
				return CreateShaderResourceView(handle, view_desc);
			case RG_UAV:
				return CreateUnorderedAccessView(handle, view_desc);
			case RG_RTV:
				return CreateRenderTargetView(handle, view_desc);
			case RG_DSV:
				return CreateDepthStencilView(handle, view_desc);
			default:
				ADRIA_ASSERT(false && "Invalid View Type");
			}

			return  RGResourceView{ .ptr = NULL };
		}

		template<RGResourceViewType ViewType>
		RGResourceView CreateBufferView(RGBufferHandle handle, BufferViewDesc const& view_desc)
		{
			switch (ViewType)
			{
			case RG_SRV:
				return CreateShaderResourceView(handle, view_desc);
			case RG_UAV:
				return CreateUnorderedAccessView(handle, view_desc);
			case RG_RTV:
			case RG_DSV:
			default:
				ADRIA_ASSERT(false && "Invalid View Type");
			}

			return  RGResourceView{ .ptr = NULL };
		}

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass);

		RGResourceView CreateShaderResourceView(RGTextureHandle handle, TextureViewDesc  const& desc);
		RGResourceView CreateRenderTargetView(RGTextureHandle handle, TextureViewDesc    const& desc);
		RGResourceView CreateUnorderedAccessView(RGTextureHandle handle, TextureViewDesc const& desc);
		RGResourceView CreateDepthStencilView(RGTextureHandle handle, TextureViewDesc    const& desc);

		RGResourceView CreateShaderResourceView(RGBufferHandle handle, BufferViewDesc  const& desc);
		RGResourceView CreateUnorderedAccessView(RGBufferHandle handle, BufferViewDesc const& desc);
	};
}