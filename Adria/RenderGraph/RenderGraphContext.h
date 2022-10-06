#pragma once
#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"
#include "RenderGraphResourceId.h"
#include "RenderGraphBlackboard.h"
#if RG_DEBUG
#include "../Utilities/StringUtil.h"
#endif

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
		RenderGraphResource(size_t id, bool imported, char const* name)
			: id(id), imported(imported), version(0), ref_count(0), name(name)
		{}

		size_t id;
		bool imported;
		size_t version;
		size_t ref_count;

		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
		char const* name = "";
	};

	template<ERGResourceType ResourceType>
	struct TypedRenderGraphResource : RenderGraphResource
	{
		using Resource = RGResourceTraits<ResourceType>::Resource;
		using ResourceDesc = RGResourceTraits<ResourceType>::ResourceDesc;

		TypedRenderGraphResource(size_t id, Resource* resource, char const* name = "")
			: RenderGraphResource(id, true, name), resource(resource), desc(resource->GetDesc())
		{}

		TypedRenderGraphResource(size_t id, ResourceDesc const& desc, char const* name = "")
			: RenderGraphResource(id, false, name), resource(nullptr), desc(desc)
		{}

		void SetName()
		{
#if RG_DEBUG
			ADRIA_ASSERT(resource != nullptr && "Call SetDebugName at allocation/creation of a resource");
			resource->SetName(name);
#endif
		}

		Resource* resource;
		ResourceDesc desc;
	};
	using RGTexture = TypedRenderGraphResource<ERGResourceType::Texture>;
	using RGBuffer = TypedRenderGraphResource<ERGResourceType::Buffer>;
	using DescriptorCPU = D3D12_CPU_DESCRIPTOR_HANDLE;
	using CommandList = ID3D12GraphicsCommandList4;

	class RenderGraphContext
	{
		friend RenderGraph;
	public:
		RenderGraphContext() = delete;
		RenderGraphContext(RenderGraphContext const&) = delete;
		RenderGraphContext& operator=(RenderGraphContext const&) = delete;

		RGBlackboard& GetBlackboard();

		Texture const& GetTexture(RGTextureId res_id) const;
		Buffer  const& GetBuffer(RGBufferId res_id) const;

		Texture const& GetCopySrcTexture(RGTextureCopySrcId res_id) const;
		Texture const& GetCopyDstTexture(RGTextureCopyDstId res_id) const;
		Buffer  const& GetCopySrcBuffer(RGBufferCopySrcId res_id) const;
		Buffer  const& GetCopyDstBuffer(RGBufferCopyDstId res_id) const;
		Buffer  const& GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const;
		Buffer  const& GetVertexBuffer(RGBufferVertexId res_id) const;
		Buffer  const& GetIndexBuffer(RGBufferIndexId res_id) const;
		Buffer  const& GetConstantBuffer(RGBufferConstantId res_id) const;

		DescriptorCPU GetRenderTarget(RGRenderTargetId res_id) const;
		DescriptorCPU GetDepthStencil(RGDepthStencilId res_id) const;
		DescriptorCPU GetReadOnlyTexture(RGTextureReadOnlyId res_id) const;
		DescriptorCPU GetReadWriteTexture(RGTextureReadWriteId res_id) const;

		DescriptorCPU GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const;
		DescriptorCPU GetReadWriteBuffer(RGBufferReadWriteId res_id) const;

		DynamicAllocation& GetAllocation(RGAllocationId);
	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphContext(RenderGraph& rg, RenderGraphPassBase& rg_pass);
	};
	using RGContext = RenderGraphContext;
}