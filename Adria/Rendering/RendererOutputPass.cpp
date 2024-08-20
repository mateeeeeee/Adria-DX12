#include "RendererOutputPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommon.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	RendererOutputPass::RendererOutputPass(GfxDevice* gfx, uint32 width, uint32 height) : gfx(gfx), width(width), height(height)
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
												data.ambient_occlusion.IsValid() ? context.GetReadOnlyTexture(data.ambient_occlusion) : gfxcommon::GetCommonView(GfxCommonViewType::WhiteTexture2D_SRV),
												context.GetReadWriteTexture(data.output) };

				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				uint32 i = dst_handle.GetIndex();
				gfx->CopyDescriptors(dst_handle, src_handles);

				float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearUAV(context.GetTexture(*data.output), gfx->GetDescriptorGPU(i + 5),
					context.GetReadWriteTexture(data.output), clear);

				struct RendererOutputConstants
				{
					uint32 normal_metallic_idx;
					uint32 diffuse_idx;
					uint32 depth_idx;
					uint32 emissive_idx;
					uint32 ao_idx;
					uint32 output_idx;
				} constants =
				{
					.normal_metallic_idx = i, .diffuse_idx = i + 1, .depth_idx = i + 2, .emissive_idx = i + 3, .ao_idx = i + 4, .output_idx = i + 5
				};

				cmd_list->SetPipelineState(renderer_output_psos->Get((uint32)type));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute);
	}

	void RendererOutputPass::CreatePSOs()
	{
		using enum RendererOutput;
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_RendererOutput;
		renderer_output_psos = std::make_unique<GfxComputePipelineStatePermutations>(PERMUTATION_COUNT, compute_pso_desc);
		renderer_output_psos->AddDefine<(uint32)Diffuse>("OUTPUT_DIFFUSE", "1");
		renderer_output_psos->AddDefine<(uint32)WorldNormal>("OUTPUT_NORMALS", "1");
		renderer_output_psos->AddDefine<(uint32)Roughness>("OUTPUT_ROUGHNESS", "1");
		renderer_output_psos->AddDefine<(uint32)Metallic>("OUTPUT_METALLIC", "1");
		renderer_output_psos->AddDefine<(uint32)Emissive>("OUTPUT_EMISSIVE", "1");
		renderer_output_psos->AddDefine<(uint32)AmbientOcclusion>("OUTPUT_AO", "1");
		renderer_output_psos->AddDefine<(uint32)IndirectLighting>("OUTPUT_INDIRECT", "1");
		renderer_output_psos->Finalize(gfx);
	}

}