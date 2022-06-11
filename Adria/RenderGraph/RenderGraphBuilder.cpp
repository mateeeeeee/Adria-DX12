#include "RenderGraphBuilder.h"
#include "RenderGraph.h"

namespace adria
{
	void RenderGraphBuilder::SetViewport(uint32 width, uint32 height)
	{
		rg_pass.viewport_width = width;
		rg_pass.viewport_height = height;
	}

	RenderGraphBuilder::RenderGraphBuilder(RenderGraph& rg, RenderGraphPassBase& rg_pass)
		: rg(rg), rg_pass(rg_pass)
	{}

	bool RenderGraphBuilder::IsTextureDeclared(RGResourceName name)
	{
		return rg.IsTextureDeclared(name);
	}

	void RenderGraphBuilder::DeclareTexture(RGResourceName name, TextureDesc const& desc)
	{
		rg_pass.creates.insert(rg.DeclareTexture(name, desc));
	}

	void RenderGraphBuilder::DeclareBuffer(RGResourceName name, BufferDesc const& desc)
	{
		ADRIA_ASSERT(false && "Not yet implemented!");
	}

	void RenderGraphBuilder::DeclareAllocation(RGResourceName name, AllocDesc const& desc)
	{
		rg.DeclareAllocation(name, desc);
	}

	void RenderGraphBuilder::DummyWriteTexture(RGResourceName name)
	{
		ADRIA_ASSERT(rg_pass.type != ERGPassType::Copy && "Invalid Call in Copy Pass");
		rg_pass.writes.insert(rg.GetTextureId(name));
	}

	void RenderGraphBuilder::DummyReadTexture(RGResourceName name)
	{
		ADRIA_ASSERT(rg_pass.type != ERGPassType::Copy && "Invalid Call in Copy Pass");
		rg_pass.reads.insert(rg.GetTextureId(name));
	}

	RGTextureCopySrcId RenderGraphBuilder::ReadCopySrcTexture(RGResourceName name)
	{
		RGTextureCopySrcId copy_src_id = rg.ReadCopySrcTexture(name);
		RGTextureId res_id(copy_src_id);
		rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		rg_pass.reads.insert(res_id);
		return copy_src_id;
	}

	RGTextureCopyDstId RenderGraphBuilder::WriteCopyDstTexture(RGResourceName name)
	{
		RGTextureCopyDstId copy_dst_id = rg.WriteCopyDstTexture(name);
		RGTextureId res_id(copy_dst_id);
		rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_COPY_DEST;
		rg_pass.writes.insert(res_id);
		return copy_dst_id;
	}

	RGTextureReadOnlyId RenderGraphBuilder::ReadTexture(RGResourceName name, ERGReadAccess read_access, TextureViewDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != ERGPassType::Copy && "Invalid Call in Copy Pass");
		RGTextureReadOnlyId read_only_id = rg.ReadTexture(name, desc);
		RGTextureId res_id = read_only_id.GetResourceId();
		switch (read_access)
		{
		case ReadAccess_PixelShader:
			rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			break;
		case ReadAccess_NonPixelShader:
			rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			break;
		case ReadAccess_AllShader:
			rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Read Flag!");
		}
		
		rg_pass.reads.insert(res_id);
		return read_only_id;
	}

	RGTextureReadWriteId RenderGraphBuilder::WriteTexture(RGResourceName name, TextureViewDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != ERGPassType::Copy && "Invalid Call in Copy Pass");
		RGTextureReadWriteId read_write_id = rg.WriteTexture(name, desc);
		RGTextureId res_id = read_write_id.GetResourceId();
		rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		auto* texture = rg.GetRGTexture(res_id);
		if (texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		if (!rg_pass.creates.contains(res_id))
		{
			++texture->version;
		}
		rg_pass.writes.insert(res_id);
		return read_write_id;
	}

	RGRenderTargetId RenderGraphBuilder::WriteRenderTarget(RGResourceName name, ERGLoadStoreAccessOp load_store_op, TextureViewDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != ERGPassType::Copy && "Invalid Call in Copy Pass");
		RGRenderTargetId render_target_id = rg.RenderTarget(name, desc);
		RGTextureId res_id = render_target_id.GetResourceId();
		rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		rg_pass.render_targets_info.push_back(RenderGraphPassBase::RenderTargetInfo{ .render_target_handle = render_target_id, .render_target_access = load_store_op });
		auto* rg_texture = rg.GetRGTexture(res_id);
		if (rg_texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;

		if (!rg_pass.creates.contains(res_id))
		{
			++rg_texture->version;
		}
		rg_pass.writes.insert(res_id);
		return render_target_id;
	}

	RGDepthStencilId RenderGraphBuilder::WriteDepthStencil(RGResourceName name, ERGLoadStoreAccessOp load_store_op, ERGLoadStoreAccessOp stencil_load_store_op /*= ERGLoadStoreAccessOp::NoAccess_NoAccess*/, TextureViewDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != ERGPassType::Copy && "Invalid Call in Copy Pass");
		RGDepthStencilId depth_stencil_id = rg.DepthStencil(name, desc);
		RGTextureId res_id = depth_stencil_id.GetResourceId();

		rg_pass.depth_stencil = RenderGraphPassBase::DepthStencilInfo{ .depth_stencil_handle = depth_stencil_id, .depth_access = load_store_op,.stencil_access = stencil_load_store_op, .readonly = false };
		auto* rg_texture = rg.GetRGTexture(res_id);

		if (!rg_pass.creates.contains(res_id))
		{
			++rg_texture->version;
		}
		rg_pass.writes.insert(res_id);

		if (rg_texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		return depth_stencil_id;
	}

	RGDepthStencilId RenderGraphBuilder::ReadDepthStencil(RGResourceName name, ERGLoadStoreAccessOp load_store_op, ERGLoadStoreAccessOp stencil_load_store_op /*= ERGLoadStoreAccessOp::NoAccess_NoAccess*/, TextureViewDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != ERGPassType::Copy && "Invalid Call in Copy Pass");
		RGDepthStencilId depth_stencil_id = rg.DepthStencil(name, desc);
		RGTextureId res_id = depth_stencil_id.GetResourceId();

		rg_pass.depth_stencil = RenderGraphPassBase::DepthStencilInfo{ .depth_stencil_handle = depth_stencil_id, .depth_access = load_store_op,.stencil_access = stencil_load_store_op, .readonly = false };
		auto* rg_texture = rg.GetRGTexture(res_id);
		rg_pass.reads.insert(res_id);

		if (rg_texture->imported) rg_pass.flags |= ERGPassFlags::ForceNoCull;
		rg_pass.resource_state_map[res_id] |= D3D12_RESOURCE_STATE_DEPTH_READ;
		return depth_stencil_id;
	}

	RGAllocationId RenderGraphBuilder::UseAllocation(RGResourceName name)
	{
		return rg.UseAllocation(name);
	}
}