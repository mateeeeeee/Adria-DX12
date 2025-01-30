#include "RendererOutputPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	RendererOutputPass::RendererOutputPass(GfxDevice* gfx, Uint32 width, Uint32 height) : gfx(gfx), width(width), height(height)
	{
		CreatePSOs();
	}

	RendererOutputPass::~RendererOutputPass() {}

	void RendererOutputPass::AddPass(RenderGraph& rg, RendererOutput type)
	{
		ADRIA_ASSERT(type != RendererOutput::Final); 

		struct RendererOutputPassData
		{
			RGTextureReadOnlyId  gbuffer_normal;
			RGTextureReadOnlyId  gbuffer_albedo;
			RGTextureReadOnlyId  gbuffer_emissive;
			RGTextureReadOnlyId  gbuffer_custom;
			RGTextureReadOnlyId  depth;
			RGTextureReadOnlyId  ambient_occlusion;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.AddPass<RendererOutputPassData>("Renderer Output Pass",
			[=](RendererOutputPassData& data, RenderGraphBuilder& builder)
			{
				data.output = builder.WriteTexture(RG_NAME(FinalTexture));
				data.gbuffer_normal = builder.ReadTexture(RG_NAME(GBufferNormal), ReadAccess_NonPixelShader);
				data.gbuffer_albedo = builder.ReadTexture(RG_NAME(GBufferAlbedo), ReadAccess_NonPixelShader);
				data.gbuffer_emissive = builder.ReadTexture(RG_NAME(GBufferEmissive), ReadAccess_NonPixelShader);
				data.gbuffer_custom = builder.ReadTexture(RG_NAME(GBufferCustom), ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_NAME(AmbientOcclusion))) data.ambient_occlusion = builder.ReadTexture(RG_NAME(AmbientOcclusion), ReadAccess_NonPixelShader);
				else data.ambient_occlusion.Invalidate();
			},
			[=](RendererOutputPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.gbuffer_normal),
												context.GetReadOnlyTexture(data.gbuffer_albedo),
												context.GetReadOnlyTexture(data.depth),
												context.GetReadOnlyTexture(data.gbuffer_emissive),
												context.GetReadOnlyTexture(data.gbuffer_custom),
												data.ambient_occlusion.IsValid() ? context.GetReadOnlyTexture(data.ambient_occlusion) : gfxcommon::GetCommonView(GfxCommonViewType::WhiteTexture2D_SRV),
												context.GetReadWriteTexture(data.output) };

				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				Uint32 i = dst_handle.GetIndex();
				gfx->CopyDescriptors(dst_handle, src_handles);

				Float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearUAV(context.GetTexture(*data.output), gfx->GetDescriptorGPU(i + 5),
					context.GetReadWriteTexture(data.output), clear);

				struct RendererOutputConstants
				{
					Uint32 normal_metallic_idx;
					Uint32 diffuse_idx;
					Uint32 depth_idx;
					Uint32 emissive_idx;
					Uint32 custom_idx;
					Uint32 ao_idx;
					Uint32 output_idx;
				} constants =
				{
					.normal_metallic_idx = i, .diffuse_idx = i + 1, .depth_idx = i + 2, .emissive_idx = i + 3, 
					.custom_idx = i + 4, .ao_idx = i + 5, .output_idx = i + 6
				};

				static std::array<Char const*, (Uint32)RendererOutput::Count> OutputDefines =
				{
					"",
					"OUTPUT_DIFFUSE",
					"OUTPUT_NORMALS",
					"OUTPUT_DEPTH",
					"OUTPUT_ROUGHNESS",
					"OUTPUT_METALLIC",
					"OUTPUT_EMISSIVE",
					"OUTPUT_AO",
					"OUTPUT_INDIRECT",
					"OUTPUT_SHADING_EXTENSION",
					"OUTPUT_CUSTOM",
					"OUTPUT_MIPMAPS"
				};
				renderer_output_psos->AddDefine(OutputDefines[(Uint32)type], "1");

				cmd_list->SetPipelineState(renderer_output_psos->Get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void RendererOutputPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_RendererOutput;
		renderer_output_psos = std::make_unique<GfxComputePipelineStatePermutations>(gfx, compute_pso_desc);
	}

}