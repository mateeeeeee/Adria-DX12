#pragma once
#include <string>
#include "RenderGraphResourceRef.h"
#include "RenderGraphBlackboard.h"

namespace adria
{
	class RenderGraphPassBase;
	class RenderGraph;

	template<ERGResourceType ResourceType>
	struct RGResourceTraits;

	template<>
	struct RGResourceTraits<ERGResourceType::Texture>
	{
		using Resource = Texture;
		using ResourceDesc = TextureDesc;
	};

	template<>
	struct RGResourceTraits<ERGResourceType::Buffer>
	{
		using Resource = Buffer;
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

		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
	};

	template<ERGResourceType ResourceType>
	struct TypedRenderGraphResource : RenderGraphResource
	{
		using Resource = RGResourceTraits<ResourceType>::Resource;
		using ResourceDesc = RGResourceTraits<ResourceType>::ResourceDesc;

		TypedRenderGraphResource(char const* name, size_t id, Resource* resource)
			: RenderGraphResource(name, id, true), resource(resource), desc(resource->GetDesc())
		{}

		TypedRenderGraphResource(char const* name, size_t id, ResourceDesc const& desc)
			: RenderGraphResource(name, id, false), resource(nullptr), desc(desc)
		{}

		Resource* resource;
		ResourceDesc desc;
	};
	using RGTexture = TypedRenderGraphResource<ERGResourceType::Texture>;
	using RGBuffer = TypedRenderGraphResource<ERGResourceType::Buffer>;

	using ResourceView = D3D12_CPU_DESCRIPTOR_HANDLE;
	using ResourceState = D3D12_RESOURCE_STATES;
	using CommandList = ID3D12GraphicsCommandList4;

	class RenderGraphResources
	{
		friend RenderGraph;
	public:
		RenderGraphResources() = delete;
		RenderGraphResources(RenderGraphResources const&) = delete;
		RenderGraphResources& operator=(RenderGraphResources const&) = delete;

		Texture& GetTexture(RGTextureRef handle);
		Buffer& GetBuffer(RGBufferRef handle);

		ResourceView GetSRV(RGTextureSRVRef handle) const;
		ResourceView GetUAV(RGTextureUAVRef handle) const;
		ResourceView GetRTV(RGTextureRTVRef handle) const;
		ResourceView GetDSV(RGTextureDSVRef handle) const;

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass);
	};
}