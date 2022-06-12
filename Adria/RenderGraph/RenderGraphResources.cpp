#include "RenderGraphResources.h"
#include "RenderGraph.h"

namespace adria
{

	RenderGraphResources::RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass) : rg(rg), rg_pass(rg_pass)
	{}

	RGBlackboard& RenderGraphResources::GetBlackboard()
	{
		return rg.GetBlackboard();
	}

	Texture const& RenderGraphResources::GetTexture(RGTextureId res_id) const
	{
		return *rg.GetTexture(res_id);
	}

	Buffer const& RenderGraphResources::GetBuffer(RGBufferId res_id) const
	{
		return *rg.GetBuffer(res_id);
	}

	Texture const& RenderGraphResources::GetCopySrcTexture(RGTextureCopySrcId res_id) const
	{
		return rg.GetCopySrcTexture(res_id);
	}

	Texture const& RenderGraphResources::GetCopyDstTexture(RGTextureCopyDstId res_id) const
	{
		return rg.GetCopyDstTexture(res_id);
	}


	Buffer const& RenderGraphResources::GetCopySrcBuffer(RGBufferCopySrcId res_id) const
	{
		return rg.GetCopySrcBuffer(res_id);
	}

	Buffer const& RenderGraphResources::GetCopyDstBuffer(RGBufferCopyDstId res_id) const
	{
		return rg.GetCopyDstBuffer(res_id);
	}

	Buffer const& RenderGraphResources::GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const
	{
		return rg.GetIndirectArgsBuffer(res_id);
	}

	RGDescriptor RenderGraphResources::GetRenderTarget(RGRenderTargetId res_id) const
	{
		return rg.GetRenderTarget(res_id);
	}

	RGDescriptor RenderGraphResources::GetDepthStencil(RGDepthStencilId res_id) const
	{
		return rg.GetDepthStencil(res_id);
	}

	RGDescriptor RenderGraphResources::GetReadOnlyTexture(RGTextureReadOnlyId res_id) const
	{
		return rg.GetReadOnlyTexture(res_id);
	}

	RGDescriptor RenderGraphResources::GetReadWriteTexture(RGTextureReadWriteId res_id) const
	{
		return rg.GetReadWriteTexture(res_id);
	}

	RGDescriptor RenderGraphResources::GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const
	{
		return rg.GetReadOnlyBuffer(res_id);
	}

	RGDescriptor RenderGraphResources::GetReadWriteBuffer(RGBufferReadWriteId res_id) const
	{
		return rg.GetReadWriteBuffer(res_id);
	}

	DynamicAllocation& RenderGraphResources::GetAllocation(RGAllocationId alloc_id)
	{
		return rg.GetAllocation(alloc_id);
	}

}

