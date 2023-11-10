#include "HelperPasses.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{
	void CopyToTexturePass::AddPass(RenderGraph& rendergraph, RGResourceName texture_dst, RGResourceName texture_src, BlendMode mode)
	{
		struct CopyToTexturePassData
		{
			RGTextureReadOnlyId texture_src;
		};

		rendergraph.AddPass<CopyToTexturePassData>("Copy To Texture Pass",
			[=](CopyToTexturePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(texture_dst, RGLoadStoreAccessOp::Preserve_Preserve);
				data.texture_src = builder.ReadTexture(texture_src, ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CopyToTexturePassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				switch (mode)
				{
				case BlendMode::None:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Copy));
					break;
				case BlendMode::AlphaBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Copy_AlphaBlend));
					break;
				case BlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Copy_AdditiveBlend));
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
				}

				GfxDescriptor dst = gfx->AllocateDescriptorsGPU();
				gfx->CopyDescriptors(1, dst, context.GetReadOnlyTexture(data.texture_src));

				cmd_list->SetRootConstant(1, dst.GetIndex(), 0);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
				cmd_list->Draw(3);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void CopyToTexturePass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void CopyToTexturePass::SetResolution(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	AddTexturesPass::AddTexturesPass(uint32 w, uint32 h) : width(w), height(h) {}

	void AddTexturesPass::AddPass(RenderGraph& rendergraph, RGResourceName texture_dst, RGResourceName texture_src1, RGResourceName texture_src2, BlendMode mode /*= EBlendMode::None*/)
	{
		struct CopyToTexturePassData
		{
			RGTextureReadOnlyId texture_src1;
			RGTextureReadOnlyId texture_src2;
		};

		rendergraph.AddPass<CopyToTexturePassData>("Add Textures Pass",
			[=](CopyToTexturePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(texture_dst, RGLoadStoreAccessOp::Preserve_Preserve);
				data.texture_src1 = builder.ReadTexture(texture_src1, ReadAccess_PixelShader);
				data.texture_src2 = builder.ReadTexture(texture_src2, ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CopyToTexturePassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				switch (mode)
				{
				case BlendMode::None:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Add));
					break;
				case BlendMode::AlphaBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Add_AlphaBlend));
					break;
				case BlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Add_AdditiveBlend));
					break;
				default:
					ADRIA_ASSERT(false && "Invalid Copy Mode in CopyTexture");
				}

				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(2);
				GfxDescriptor src_descriptors[] = { context.GetReadOnlyTexture(data.texture_src1), context.GetReadOnlyTexture(data.texture_src2) };
				uint32 i = dst_descriptor.GetIndex();
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);

				cmd_list->SetRootConstant(1, i, 0);
				cmd_list->SetRootConstant(1, i + 1, 1);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
				cmd_list->Draw(3);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void AddTexturesPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void AddTexturesPass::SetResolution(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void TextureChannelMappingPass::AddPass(RenderGraph& rendergraph, RGResourceName texture_dst, RGResourceName texture_src, GfxTextureChannelMapping mapping)
	{
		struct TextureChannelMappingPassData
		{
			RGTextureReadOnlyId texture_src;
		};

		rendergraph.AddPass<TextureChannelMappingPassData>("Texture Channel Mapping Pass",
			[=](TextureChannelMappingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc dst_desc = builder.GetTextureDesc(texture_src);
				dst_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(texture_dst, dst_desc);

				builder.WriteRenderTarget(texture_dst, RGLoadStoreAccessOp::Clear_Preserve);
				data.texture_src = builder.ReadTexture(texture_src, mapping, ReadAccess_PixelShader);
				builder.SetViewport(dst_desc.width, dst_desc.height);
			},
			[=](TextureChannelMappingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Copy));

				GfxTexture& src_tex = context.GetTexture(*data.texture_src);
				GfxDescriptor dst = gfx->AllocateDescriptorsGPU();
				gfx->CopyDescriptors(1, dst, context.GetReadOnlyTexture(data.texture_src));

				cmd_list->SetRootConstant(1, dst.GetIndex(), 0);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
				cmd_list->Draw(3);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

}
