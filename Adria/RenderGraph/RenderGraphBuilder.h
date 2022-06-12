#pragma once
#include "RenderGraphResourceName.h"
#include "RenderGraphPass.h"
#include "RenderGraphContext.h"

namespace adria
{
	class RenderGraphBuilder
	{
		friend class RenderGraph;

	public:
		RenderGraphBuilder() = delete;
		RenderGraphBuilder(RenderGraphBuilder const&) = delete;
		RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

		bool IsTextureDeclared(RGResourceName name);
		void DeclareTexture(RGResourceName name, TextureDesc const& desc);
		void DeclareBuffer(RGResourceName name, BufferDesc const& desc);
		void DeclareAllocation(RGResourceName name, AllocDesc const& desc);

		void DummyWriteTexture(RGResourceName name);
		void DummyReadTexture(RGResourceName name);
		void DummyReadBuffer(RGResourceName name);
		void DummyWriteBuffer(RGResourceName name);

		[[nodiscard]] RGTextureCopySrcId ReadCopySrcTexture(RGResourceName name);
		[[nodiscard]] RGTextureCopyDstId WriteCopyDstTexture(RGResourceName name);
		[[nodiscard]] RGTextureReadOnlyId ReadTexture(RGResourceName name, ERGReadAccess read_access, TextureViewDesc const& desc = {});
		[[nodiscard]] RGTextureReadWriteId WriteTexture(RGResourceName name, TextureViewDesc const& desc = {});
		[[maybe_unused]] RGRenderTargetId WriteRenderTarget(RGResourceName name, ERGLoadStoreAccessOp load_store_op, TextureViewDesc const& desc = {});
		[[maybe_unused]] RGDepthStencilId WriteDepthStencil(RGResourceName name, ERGLoadStoreAccessOp load_store_op, ERGLoadStoreAccessOp stencil_load_store_op = ERGLoadStoreAccessOp::NoAccess_NoAccess, TextureViewDesc const& desc = {});
		[[maybe_unused]] RGDepthStencilId ReadDepthStencil(RGResourceName name, ERGLoadStoreAccessOp load_store_op, ERGLoadStoreAccessOp stencil_load_store_op = ERGLoadStoreAccessOp::NoAccess_NoAccess, TextureViewDesc const& desc = {});

		[[nodiscard]] RGBufferCopySrcId ReadCopySrcBuffer(RGResourceName name);
		[[nodiscard]] RGBufferCopyDstId WriteCopyDstBuffer(RGResourceName name);
		[[nodiscard]] RGBufferIndirectArgsId ReadIndirectArgsBuffer(RGResourceName name);
		[[nodiscard]] RGBufferReadOnlyId ReadBuffer(RGResourceName name, ERGReadAccess read_access, BufferViewDesc const& desc = {});
		[[nodiscard]] RGBufferReadWriteId WriteBuffer(RGResourceName name, BufferViewDesc const& desc = {});

		[[nodiscard]] RGAllocationId UseAllocation(RGResourceName name);
		
		void SetViewport(uint32 width, uint32 height);
	private:
		RenderGraphBuilder(RenderGraph&, RenderGraphPassBase&);

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;
	};

	using RGBuilder = RenderGraphBuilder;
}