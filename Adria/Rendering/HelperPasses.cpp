#include "HelperPasses.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 

#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	CopyToTexturePass::CopyToTexturePass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}

	CopyToTexturePass::~CopyToTexturePass()
	{
	}

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
					cmd_list->SetPipelineState(copy_psos->Get<0>());
					break;
				case BlendMode::AlphaBlend:
					cmd_list->SetPipelineState(copy_psos->Get<1>());
					break;
				case BlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(copy_psos->Get<2>());
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

	void CopyToTexturePass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_FullscreenTriangle;
		gfx_pso_desc.PS = PS_Copy;
		gfx_pso_desc.num_render_targets = 1;
		gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;

		copy_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(3, gfx_pso_desc);
		copy_psos->ModifyDesc<1>([](GfxGraphicsPipelineStateDesc& desc)
			{
				desc.blend_state.render_target[0].blend_enable = true;
				desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
				desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
				desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
			});
		copy_psos->ModifyDesc<2>([](GfxGraphicsPipelineStateDesc& desc)
			{
				desc.blend_state.render_target[0].blend_enable = true;
				desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
			});
		copy_psos->Finalize(gfx);
	}

	AddTexturesPass::AddTexturesPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h) 
	{
		CreatePSOs();
	}

	AddTexturesPass::~AddTexturesPass()
	{
	}

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
					cmd_list->SetPipelineState(add_psos->Get<0>());
					break;
				case BlendMode::AlphaBlend:
					cmd_list->SetPipelineState(add_psos->Get<1>());
					break;
				case BlendMode::AdditiveBlend:
					cmd_list->SetPipelineState(add_psos->Get<2>());
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

	void AddTexturesPass::CreatePSOs()
	{
		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_FullscreenTriangle;
		gfx_pso_desc.PS = PS_Add;
		gfx_pso_desc.num_render_targets = 1;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;

		add_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(3, gfx_pso_desc);
		add_psos->ModifyDesc<1>([](GfxGraphicsPipelineStateDesc& desc)
			{
				desc.blend_state.render_target[0].blend_enable = true;
				desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
				desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
				desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
			});
		add_psos->ModifyDesc<2>([](GfxGraphicsPipelineStateDesc& desc)
			{
				desc.blend_state.render_target[0].blend_enable = true;
				desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
			});
		add_psos->Finalize(gfx);
	}
}
