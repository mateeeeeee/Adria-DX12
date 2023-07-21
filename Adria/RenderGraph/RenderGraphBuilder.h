#pragma once
#include "RenderGraphResourceName.h"
#include "RenderGraphPass.h"
#include "RenderGraphContext.h"

namespace adria
{
	struct RGTextureDesc
	{
		GfxTextureType type = GfxTextureType_2D;
		uint32 width = 0;
		uint32 height = 0;
		uint32 depth = 0;
		uint32 array_size = 1;
		uint32 mip_levels = 1;
		uint32 sample_count = 1;
		GfxResourceUsage heap_type = GfxResourceUsage::Default;
		GfxTextureMiscFlag misc_flags = GfxTextureMiscFlag::None;
		GfxClearValue clear_value{};
		GfxFormat format = GfxFormat::UNKNOWN;
	};
	struct RGBufferDesc
	{
		uint64 size = 0;
		uint32 stride = 0;
		GfxResourceUsage resource_usage = GfxResourceUsage::Default;
		GfxBufferMiscFlag misc_flags = GfxBufferMiscFlag::None;
		GfxFormat format = GfxFormat::UNKNOWN;
	};

	inline void InitGfxTextureDesc(RGTextureDesc const& rg_tex_desc, GfxTextureDesc& tex_desc)
	{
		tex_desc.type = rg_tex_desc.type;
		tex_desc.width = rg_tex_desc.width;
		tex_desc.height = rg_tex_desc.height;
		tex_desc.depth = rg_tex_desc.depth;
		tex_desc.array_size = rg_tex_desc.array_size;
		tex_desc.mip_levels = rg_tex_desc.mip_levels;
		tex_desc.format = rg_tex_desc.format;
		tex_desc.sample_count = rg_tex_desc.sample_count;
		tex_desc.heap_type = rg_tex_desc.heap_type;
		tex_desc.misc_flags = rg_tex_desc.misc_flags;
		tex_desc.clear_value = rg_tex_desc.clear_value;
		tex_desc.bind_flags = GfxBindFlag::None;
		tex_desc.initial_state = GfxResourceState::Common;
	}
	inline void InitGfxBufferDesc(RGBufferDesc const& rg_buf_desc, GfxBufferDesc& buf_desc)
	{
		buf_desc.size = rg_buf_desc.size;
		buf_desc.stride = rg_buf_desc.stride;
		buf_desc.resource_usage = rg_buf_desc.resource_usage;
		buf_desc.misc_flags = rg_buf_desc.misc_flags;
		buf_desc.format = rg_buf_desc.format;
		buf_desc.bind_flags = GfxBindFlag::None;
	}
	inline void ExtractRGTextureDesc(GfxTextureDesc const& tex_desc, RGTextureDesc& rg_tex_desc)
	{
		rg_tex_desc.type = tex_desc.type;
		rg_tex_desc.width = tex_desc.width;
		rg_tex_desc.height = tex_desc.height;
		rg_tex_desc.depth = tex_desc.depth;
		rg_tex_desc.array_size = tex_desc.array_size;
		rg_tex_desc.mip_levels = tex_desc.mip_levels;
		rg_tex_desc.format = tex_desc.format;
		rg_tex_desc.sample_count = tex_desc.sample_count;
		rg_tex_desc.heap_type = tex_desc.heap_type;
		rg_tex_desc.misc_flags = tex_desc.misc_flags;
		rg_tex_desc.clear_value = tex_desc.clear_value;
	}
	inline void ExtractRGBufferDesc(GfxBufferDesc const& buf_desc, RGBufferDesc& rg_buf_desc)
	{
		rg_buf_desc.size = buf_desc.size;
		rg_buf_desc.stride = buf_desc.stride;
		rg_buf_desc.resource_usage = buf_desc.resource_usage;
		rg_buf_desc.misc_flags = buf_desc.misc_flags;
		rg_buf_desc.format = buf_desc.format;
	}

	class RenderGraphBuilder
	{
		friend class RenderGraph;

	public:
		RenderGraphBuilder() = delete;
		RenderGraphBuilder(RenderGraphBuilder const&) = delete;
		RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

		bool IsTextureDeclared(RGResourceName name) const;
		bool IsBufferDeclared(RGResourceName name) const;
		void ExportTexture(RGResourceName name, GfxTexture* texture);
		void ExportBuffer(RGResourceName name, GfxBuffer* buffer);
		void DeclareTexture(RGResourceName name, RGTextureDesc const& desc);
		void DeclareBuffer(RGResourceName name, RGBufferDesc const& desc);

		void DummyWriteTexture(RGResourceName name);
		void DummyReadTexture(RGResourceName name);
		void DummyReadBuffer(RGResourceName name);
		void DummyWriteBuffer(RGResourceName name);

		[[nodiscard]] RGTextureCopySrcId ReadCopySrcTexture(RGResourceName name);
		[[nodiscard]] RGTextureCopyDstId WriteCopyDstTexture(RGResourceName name);

		[[nodiscard]] RGBufferCopySrcId ReadCopySrcBuffer(RGResourceName name);
		[[nodiscard]] RGBufferCopyDstId WriteCopyDstBuffer(RGResourceName name);
		[[nodiscard]] RGBufferIndirectArgsId ReadIndirectArgsBuffer(RGResourceName name);
		[[nodiscard]] RGBufferVertexId ReadVertexBuffer(RGResourceName name);
		[[nodiscard]] RGBufferIndexId ReadIndexBuffer(RGResourceName name);
		[[nodiscard]] RGBufferConstantId ReadConstantBuffer(RGResourceName name);
		
		[[nodiscard]] RGTextureReadOnlyId ReadTexture(RGResourceName name, RGReadAccess read_access = ReadAccess_AllShader,
			uint32 first_mip = 0, uint32 mip_count = -1, uint32 first_slice = 0, uint32 slice_count = -1)
		{
			return ReadTextureImpl(name, read_access, GfxTextureSubresourceDesc{ first_slice, slice_count, first_mip, mip_count });
		}
		[[nodiscard]] RGTextureReadWriteId WriteTexture(RGResourceName name,
			uint32 first_mip = 0, uint32 mip_count = -1, uint32 first_slice = 0, uint32 slice_count = -1)
		{
			return WriteTextureImpl(name, GfxTextureSubresourceDesc{ first_slice, slice_count, first_mip, mip_count });
		}
		[[maybe_unused]] RGRenderTargetId WriteRenderTarget(RGResourceName name, RGLoadStoreAccessOp load_store_op, 
			uint32 first_mip = 0, uint32 mip_count = -1, uint32 first_slice = 0, uint32 slice_count = -1)
		{
			return WriteRenderTargetImpl(name, load_store_op, GfxTextureSubresourceDesc{ first_slice, slice_count, first_mip, mip_count });
		}
		[[maybe_unused]] RGDepthStencilId WriteDepthStencil(RGResourceName name, RGLoadStoreAccessOp load_store_op, 
			uint32 first_mip = 0, uint32 mip_count = -1, uint32 first_slice = 0, uint32 slice_count = -1)
		{
			return WriteDepthStencilImpl(name, load_store_op, RGLoadStoreAccessOp::NoAccess_NoAccess, GfxTextureSubresourceDesc{ first_slice, slice_count, first_mip, mip_count });
		}
		[[maybe_unused]] RGDepthStencilId ReadDepthStencil(RGResourceName name, RGLoadStoreAccessOp load_store_op,
			uint32 first_mip = 0, uint32 mip_count = -1, uint32 first_slice = 0, uint32 slice_count = -1)
		{
			return ReadDepthStencilImpl(name, load_store_op, RGLoadStoreAccessOp::NoAccess_NoAccess, GfxTextureSubresourceDesc{ first_slice, slice_count, first_mip, mip_count });
		}

		[[nodiscard]] RGBufferReadOnlyId ReadBuffer(RGResourceName name, RGReadAccess read_access = ReadAccess_AllShader, uint32 offset = 0, uint32 size = -1)
		{
			return ReadBufferImpl(name, read_access, GfxBufferSubresourceDesc{ offset, size });
		}
		[[nodiscard]] RGBufferReadWriteId WriteBuffer(RGResourceName name, uint32 offset = 0, uint32 size = -1)
		{
			return WriteBufferImpl(name, GfxBufferSubresourceDesc{ offset, size });
		}
		[[nodiscard]] RGBufferReadWriteId WriteBuffer(RGResourceName name, RGResourceName counter_name, uint32 offset = 0, uint32 size = -1)
		{
			return WriteBufferImpl(name, counter_name, GfxBufferSubresourceDesc{ offset, size });
		}

		void SetViewport(uint32 width, uint32 height);
		RGTextureDesc GetTextureDesc(RGResourceName);
		RGBufferDesc  GetBufferDesc(RGResourceName);
		void AddBufferBindFlags(RGResourceName name, GfxBindFlag flags);
		void AddTextureBindFlags(RGResourceName name, GfxBindFlag flags);
	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphBuilder(RenderGraph&, RenderGraphPassBase&);

		[[nodiscard]] RGTextureReadOnlyId ReadTextureImpl(RGResourceName name, RGReadAccess read_access, GfxTextureSubresourceDesc const& desc);
		[[nodiscard]] RGTextureReadWriteId WriteTextureImpl(RGResourceName name, GfxTextureSubresourceDesc const& desc);
		[[maybe_unused]] RGRenderTargetId WriteRenderTargetImpl(RGResourceName name, RGLoadStoreAccessOp load_store_op, GfxTextureSubresourceDesc const& desc);
		[[maybe_unused]] RGDepthStencilId WriteDepthStencilImpl(RGResourceName name, RGLoadStoreAccessOp load_store_op, RGLoadStoreAccessOp stencil_load_store_op, GfxTextureSubresourceDesc const& desc);
		[[maybe_unused]] RGDepthStencilId ReadDepthStencilImpl(RGResourceName name, RGLoadStoreAccessOp load_store_op, RGLoadStoreAccessOp stencil_load_store_op, GfxTextureSubresourceDesc const& desc);
		[[nodiscard]] RGBufferReadOnlyId ReadBufferImpl(RGResourceName name, RGReadAccess read_access, GfxBufferSubresourceDesc const& desc);
		[[nodiscard]] RGBufferReadWriteId WriteBufferImpl(RGResourceName name, GfxBufferSubresourceDesc const& desc);
		[[nodiscard]] RGBufferReadWriteId WriteBufferImpl(RGResourceName name, RGResourceName counter_name, GfxBufferSubresourceDesc const& desc);
	};

	using RGBuilder = RenderGraphBuilder;
}