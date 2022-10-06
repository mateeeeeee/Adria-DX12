#include "RenderGraphContext.h"
#include "RenderGraph.h"

namespace adria
{

	RenderGraphContext::RenderGraphContext(RenderGraph& rg, RenderGraphPassBase& rg_pass) : rg(rg), rg_pass(rg_pass)
	{}

	RGBlackboard& RenderGraphContext::GetBlackboard()
	{
		return rg.GetBlackboard();
	}

	Texture const& RenderGraphContext::GetTexture(RGTextureId res_id) const
	{
		return *rg.GetTexture(res_id);
	}

	Buffer const& RenderGraphContext::GetBuffer(RGBufferId res_id) const
	{
		return *rg.GetBuffer(res_id);
	}

	Texture const& RenderGraphContext::GetCopySrcTexture(RGTextureCopySrcId res_id) const
	{
		return rg.GetCopySrcTexture(res_id);
	}

	Texture const& RenderGraphContext::GetCopyDstTexture(RGTextureCopyDstId res_id) const
	{
		return rg.GetCopyDstTexture(res_id);
	}


	Buffer const& RenderGraphContext::GetCopySrcBuffer(RGBufferCopySrcId res_id) const
	{
		return rg.GetCopySrcBuffer(res_id);
	}

	Buffer const& RenderGraphContext::GetCopyDstBuffer(RGBufferCopyDstId res_id) const
	{
		return rg.GetCopyDstBuffer(res_id);
	}

	Buffer const& RenderGraphContext::GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const
	{
		return rg.GetIndirectArgsBuffer(res_id);
	}

	Buffer const& RenderGraphContext::GetVertexBuffer(RGBufferVertexId res_id) const
	{
		return rg.GetVertexBuffer(res_id);
	}

	Buffer const& RenderGraphContext::GetIndexBuffer(RGBufferIndexId res_id) const
	{
		return rg.GetIndexBuffer(res_id);
	}

	Buffer const& RenderGraphContext::GetConstantBuffer(RGBufferConstantId res_id) const
	{
		return rg.GetConstantBuffer(res_id);
	}

	DescriptorCPU RenderGraphContext::GetRenderTarget(RGRenderTargetId res_id) const
	{
		return rg.GetRenderTarget(res_id);
	}

	DescriptorCPU RenderGraphContext::GetDepthStencil(RGDepthStencilId res_id) const
	{
		return rg.GetDepthStencil(res_id);
	}

	DescriptorCPU RenderGraphContext::GetReadOnlyTexture(RGTextureReadOnlyId res_id) const
	{
		return rg.GetReadOnlyTexture(res_id);
	}

	DescriptorCPU RenderGraphContext::GetReadWriteTexture(RGTextureReadWriteId res_id) const
	{
		return rg.GetReadWriteTexture(res_id);
	}

	DescriptorCPU RenderGraphContext::GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const
	{
		return rg.GetReadOnlyBuffer(res_id);
	}

	DescriptorCPU RenderGraphContext::GetReadWriteBuffer(RGBufferReadWriteId res_id) const
	{
		return rg.GetReadWriteBuffer(res_id);
	}

	DynamicAllocation& RenderGraphContext::GetAllocation(RGAllocationId alloc_id)
	{
		return rg.GetAllocation(alloc_id);
	}

}

