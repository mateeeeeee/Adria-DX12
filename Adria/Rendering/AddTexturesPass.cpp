#include "AddTexturesPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	AddTexturesPass::AddTexturesPass(uint32 w, uint32 h) : width(w), height(h)
	{}

	void AddTexturesPass::AddPass(RenderGraph& rendergraph, RGResourceName render_target, RGResourceName texture1, RGResourceName texture2, BlendMode mode /*= EBlendMode::None*/)
	{
		struct CopyToTexturePassData
		{
			RGTextureReadOnlyId texture_src1;
			RGTextureReadOnlyId texture_src2;
		};

		rendergraph.AddPass<CopyToTexturePassData>("AddTextures Pass",
			[=](CopyToTexturePassData& data, RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(render_target, RGLoadStoreAccessOp::Preserve_Preserve);
				data.texture_src1 = builder.ReadTexture(texture1, ReadAccess_PixelShader);
				data.texture_src2 = builder.ReadTexture(texture2, ReadAccess_PixelShader);
				builder.SetViewport(width, height);
			},
			[=](CopyToTexturePassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				auto descriptor_allocator = gfx->GetDescriptorAllocator();

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

				GfxDescriptor dst_descriptor = descriptor_allocator->Allocate(2);
				GfxDescriptor src_descriptors[] = { context.GetReadOnlyTexture(data.texture_src1), context.GetReadOnlyTexture(data.texture_src2) };
				uint32 i = dst_descriptor.GetIndex();
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);

				cmd_list->SetRootConstant(1, i, 0);
				cmd_list->SetRootConstant(1, i + 1, 1);
				cmd_list->SetTopology(GfxPrimitiveTopology::TriangleStrip);
				cmd_list->Draw(4);
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void AddTexturesPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

}

