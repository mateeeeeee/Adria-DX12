#include "RenderGraphBuilder.h"
#include "RenderGraph.h"

namespace adria
{
	RenderGraphBuilder::RenderGraphBuilder(RenderGraph& rg, RenderGraphPassBase& rg_pass)
		: rg(rg), rg_pass(rg_pass)
	{}

	bool RenderGraphBuilder::IsTextureDeclared(RGResourceName name) const
	{
		return rg.IsTextureDeclared(name);
	}

	bool RenderGraphBuilder::IsBufferDeclared(RGResourceName name) const
	{
		return rg.IsBufferDeclared(name);
	}

	void RenderGraphBuilder::ExportTexture(RGResourceName name, GfxTexture* texture)
	{
		rg.ExportTexture(name, texture);
	}

	void RenderGraphBuilder::ExportBuffer(RGResourceName name, GfxBuffer* buffer)
	{
		rg.ExportBuffer(name, buffer);
	}

	void RenderGraphBuilder::DeclareTexture(RGResourceName name, RGTextureDesc const& desc)
	{
		rg_pass.texture_creates.insert(rg.DeclareTexture(name, desc));
	}

	void RenderGraphBuilder::DeclareBuffer(RGResourceName name, RGBufferDesc const& desc)
	{
		rg_pass.buffer_creates.insert(rg.DeclareBuffer(name, desc));
	}

	void RenderGraphBuilder::DummyWriteTexture(RGResourceName name)
	{
		rg_pass.texture_writes.insert(rg.GetTextureId(name));
	}

	void RenderGraphBuilder::DummyReadTexture(RGResourceName name)
	{
		rg_pass.texture_reads.insert(rg.GetTextureId(name));
	}

	void RenderGraphBuilder::DummyReadBuffer(RGResourceName name)
	{
		rg_pass.buffer_reads.insert(rg.GetBufferId(name));
	}

	void RenderGraphBuilder::DummyWriteBuffer(RGResourceName name)
	{
		rg_pass.buffer_writes.insert(rg.GetBufferId(name));
	}

	RGTextureCopySrcId RenderGraphBuilder::ReadCopySrcTexture(RGResourceName name)
	{
		RGTextureCopySrcId copy_src_id = rg.ReadCopySrcTexture(name);
		RGTextureId res_id(copy_src_id);
		rg_pass.texture_state_map[res_id] = GfxResourceState::CopySrc;
		rg_pass.texture_reads.insert(res_id);
		return copy_src_id;
	}

	RGTextureCopyDstId RenderGraphBuilder::WriteCopyDstTexture(RGResourceName name)
	{
		RGTextureCopyDstId copy_dst_id = rg.WriteCopyDstTexture(name);
		RGTextureId res_id(copy_dst_id);
		rg_pass.texture_state_map[res_id] = GfxResourceState::CopyDst;
		if (!rg_pass.texture_creates.contains(res_id))
		{
			DummyReadTexture(name);
		}
		rg_pass.texture_writes.insert(res_id);
		auto* texture = rg.GetRGTexture(res_id);
		if (texture->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		return copy_dst_id;
	}

	RGTextureReadOnlyId RenderGraphBuilder::ReadTextureImpl(RGResourceName name, RGReadAccess read_access, GfxTextureDescriptorDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGTextureReadOnlyId read_only_id = rg.ReadTexture(name, desc);
		RGTextureId res_id = read_only_id.GetResourceId();
		if (rg_pass.type == RGPassType::Graphics)
		{
			switch (read_access)
			{
			case ReadAccess_PixelShader:
				rg_pass.texture_state_map[res_id] = GfxResourceState::PixelSRV;
				break;
			case ReadAccess_NonPixelShader:
				rg_pass.texture_state_map[res_id] = GfxResourceState::ComputeSRV;
				break;
			case ReadAccess_AllShader:
				rg_pass.texture_state_map[res_id] = GfxResourceState::AllSRV;
				break;
			default:
				ADRIA_ASSERT_MSG(false, "Invalid Read Flag!");
			}
		}
		else if(rg_pass.type == RGPassType::Compute || rg_pass.type == RGPassType::ComputeAsync)
		{
			rg_pass.texture_state_map[res_id] = GfxResourceState::ComputeSRV;
		}
		
		rg_pass.texture_reads.insert(res_id);
		return read_only_id;
	}

	RGTextureReadWriteId RenderGraphBuilder::WriteTextureImpl(RGResourceName name, GfxTextureDescriptorDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGTextureReadWriteId read_write_id = rg.WriteTexture(name, desc);
		RGTextureId res_id = read_write_id.GetResourceId();
		rg_pass.texture_state_map[res_id] = GfxResourceState::ComputeUAV;
		if (!rg_pass.texture_creates.contains(res_id))
		{
			DummyReadTexture(name);
		}
		rg_pass.texture_writes.insert(res_id);
		auto* texture = rg.GetRGTexture(res_id);
		if (texture->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		return read_write_id;
	}

	RGRenderTargetId RenderGraphBuilder::WriteRenderTargetImpl(RGResourceName name, RGLoadStoreAccessOp load_store_op, GfxTextureDescriptorDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGRenderTargetId render_target_id = rg.RenderTarget(name, desc);
		RGTextureId res_id = render_target_id.GetResourceId();
		rg_pass.texture_state_map[res_id] = GfxResourceState::RTV;
		rg_pass.render_targets_info.push_back(RenderGraphPassBase::RenderTargetInfo{ .render_target_handle = render_target_id, .render_target_access = load_store_op });
		if (!rg_pass.texture_creates.contains(res_id))
		{
			DummyReadTexture(name);
		}
		rg_pass.texture_writes.insert(res_id);
		auto* rg_texture = rg.GetRGTexture(res_id);
		if (rg_texture->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		return render_target_id;
	}

	RGDepthStencilId RenderGraphBuilder::WriteDepthStencilImpl(RGResourceName name, RGLoadStoreAccessOp load_store_op, RGLoadStoreAccessOp stencil_load_store_op /*= ERGLoadStoreAccessOp::NoAccess_NoAccess*/, GfxTextureDescriptorDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGDepthStencilId depth_stencil_id = rg.DepthStencil(name, desc);
		RGTextureId res_id = depth_stencil_id.GetResourceId();
		rg_pass.texture_state_map[res_id] = GfxResourceState::DSV;
		rg_pass.depth_stencil = RenderGraphPassBase::DepthStencilInfo{ .depth_stencil_handle = depth_stencil_id, .depth_access = load_store_op,.stencil_access = stencil_load_store_op, .depth_read_only = false };
		if (!rg_pass.texture_creates.contains(res_id))
		{
			DummyReadTexture(name);
		}
		rg_pass.texture_writes.insert(res_id);
		auto* rg_texture = rg.GetRGTexture(res_id);
		if (rg_texture->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		return depth_stencil_id;
	}

	RGDepthStencilId RenderGraphBuilder::ReadDepthStencilImpl(RGResourceName name, RGLoadStoreAccessOp load_store_op, RGLoadStoreAccessOp stencil_load_store_op /*= ERGLoadStoreAccessOp::NoAccess_NoAccess*/, GfxTextureDescriptorDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGDepthStencilId depth_stencil_id = rg.DepthStencil(name, desc);
		RGTextureId res_id = depth_stencil_id.GetResourceId();

		rg_pass.depth_stencil = RenderGraphPassBase::DepthStencilInfo{ .depth_stencil_handle = depth_stencil_id, .depth_access = load_store_op,.stencil_access = stencil_load_store_op, .depth_read_only = true };
		auto* rg_texture = rg.GetRGTexture(res_id);
		rg_pass.texture_reads.insert(res_id);

		if (rg_texture->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		rg_pass.texture_state_map[res_id] = GfxResourceState::DSV_ReadOnly;
		return depth_stencil_id;
	}

	RGBufferCopySrcId RenderGraphBuilder::ReadCopySrcBuffer(RGResourceName name)
	{
		RGBufferCopySrcId copy_src_id = rg.ReadCopySrcBuffer(name);
		RGBufferId res_id(copy_src_id);
		rg_pass.buffer_state_map[res_id] = GfxResourceState::CopySrc;
		rg_pass.buffer_reads.insert(res_id);
		return copy_src_id;
	}

	RGBufferCopyDstId RenderGraphBuilder::WriteCopyDstBuffer(RGResourceName name)
	{
		RGBufferCopyDstId copy_dst_id = rg.WriteCopyDstBuffer(name);
		RGBufferId res_id(copy_dst_id);
		rg_pass.buffer_state_map[res_id] = GfxResourceState::CopyDst;
		if (!rg_pass.buffer_creates.contains(res_id))
		{
			DummyReadBuffer(name);
		}
		rg_pass.buffer_writes.insert(res_id);
		auto* buffer = rg.GetRGBuffer(res_id);
		if (buffer->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		return copy_dst_id;
	}

	RGBufferIndirectArgsId RenderGraphBuilder::ReadIndirectArgsBuffer(RGResourceName name)
	{
		RGBufferIndirectArgsId indirect_args_id = rg.ReadIndirectArgsBuffer(name);
		RGBufferId res_id(indirect_args_id);
		rg_pass.buffer_state_map[res_id] = GfxResourceState::IndirectArgs;
		rg_pass.buffer_reads.insert(res_id);
		return indirect_args_id;
	}

	RGBufferIndexId RenderGraphBuilder::ReadIndexBuffer(RGResourceName name)
	{
		RGBufferIndexId index_buf_id = rg.ReadVertexBuffer(name);
		RGBufferId res_id(index_buf_id);
		rg_pass.buffer_state_map[res_id] = GfxResourceState::IndexBuffer;
		rg_pass.buffer_reads.insert(res_id);
		return index_buf_id;
	}

	RGBufferReadOnlyId RenderGraphBuilder::ReadBufferImpl(RGResourceName name, RGReadAccess read_access, GfxBufferDescriptorDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGBufferReadOnlyId read_only_id = rg.ReadBuffer(name, desc);
		if (rg_pass.type == RGPassType::Compute) read_access = ReadAccess_NonPixelShader;

		RGBufferId res_id = read_only_id.GetResourceId();
		if (rg_pass.type == RGPassType::Graphics)
		{
			switch (read_access)
			{
			case ReadAccess_PixelShader:
				rg_pass.buffer_state_map[res_id] = GfxResourceState::PixelSRV;
				break;
			case ReadAccess_NonPixelShader:
				rg_pass.buffer_state_map[res_id] = GfxResourceState::ComputeSRV;
				break;
			case ReadAccess_AllShader:
				rg_pass.buffer_state_map[res_id] = GfxResourceState::AllSRV;
				break;
			default:
				ADRIA_ASSERT(false && "Invalid Read Flag!");
			}
		}
		else if (rg_pass.type == RGPassType::Compute || rg_pass.type == RGPassType::ComputeAsync)
		{
			rg_pass.buffer_state_map[res_id] = GfxResourceState::ComputeSRV;
		}
		rg_pass.buffer_reads.insert(res_id);
		return read_only_id;
	}

	RGBufferReadWriteId RenderGraphBuilder::WriteBufferImpl(RGResourceName name, GfxBufferDescriptorDesc const& desc /*= {}*/)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGBufferReadWriteId read_write_id = rg.WriteBuffer(name, desc);
		RGBufferId res_id = read_write_id.GetResourceId();
		rg_pass.buffer_state_map[res_id] = GfxResourceState::ComputeUAV;
		if (!rg_pass.buffer_creates.contains(res_id))
		{
			DummyReadBuffer(name);
		}
		rg_pass.buffer_writes.insert(res_id);
		auto* buffer = rg.GetRGBuffer(res_id);
		if (buffer->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		return read_write_id;
	}

	RGBufferReadWriteId RenderGraphBuilder::WriteBufferImpl(RGResourceName name, RGResourceName counter_name, GfxBufferDescriptorDesc const& desc)
	{
		ADRIA_ASSERT(rg_pass.type != RGPassType::Copy && "Invalid Call in Copy Pass");
		RGBufferReadWriteId read_write_id = rg.WriteBuffer(name, counter_name, desc);

		RGBufferId counter_id = rg.GetBufferId(counter_name);
		
		RGBufferId res_id = read_write_id.GetResourceId();
		rg_pass.buffer_state_map[res_id] = GfxResourceState::ComputeUAV;
		rg_pass.buffer_state_map[counter_id] = GfxResourceState::ComputeUAV;
		DummyWriteBuffer(counter_name);
		if (!rg_pass.buffer_creates.contains(res_id))
		{
			DummyReadBuffer(name);
			DummyReadBuffer(counter_name);
		}
		rg_pass.buffer_writes.insert(res_id);
		auto* buffer = rg.GetRGBuffer(res_id);
		if (buffer->imported) rg_pass.flags |= RGPassFlags::ForceNoCull;
		return read_write_id;
	}

	void RenderGraphBuilder::SetViewport(uint32 width, uint32 height)
	{
		rg_pass.viewport_width = width;
		rg_pass.viewport_height = height;
	}

	RGTextureDesc RenderGraphBuilder::GetTextureDesc(RGResourceName name)
	{
		return rg.GetTextureDesc(name);
	}

	RGBufferDesc RenderGraphBuilder::GetBufferDesc(RGResourceName name)
	{
		return rg.GetBufferDesc(name);
	}

	void RenderGraphBuilder::AddBufferBindFlags(RGResourceName name, GfxBindFlag flags)
	{
		rg.AddBufferBindFlags(name, flags);
	}

	void RenderGraphBuilder::AddTextureBindFlags(RGResourceName name, GfxBindFlag flags)
	{
		rg.AddTextureBindFlags(name, flags);
	}
}