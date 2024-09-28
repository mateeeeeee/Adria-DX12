#include "ToneMapPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "TextureManager.h"
#include "PostProcessor.h"
#include "Core/Paths.h"
#include "Core/ConsoleManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxCommon.h"
#include "Math/Packing.h"
#include "Editor/GUICommand.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{
	enum ToneMapOperator : uint8
	{
		ToneMapOperator_None,
		ToneMapOperator_Reinhard,
		ToneMapOperator_Hable,
		ToneMapOperator_Linear,
		ToneMapOperator_TonyMcMapface,
		ToneMapOperator_AgX
	};

	static TAutoConsoleVariable<int>   TonemapOperator("r.Tonemap.Operator", ToneMapOperator_TonyMcMapface, "0 - None, 1 - Reinhard, 2 - Hable, 3 - Linear, 4 - TonyMcMapface, 5 - AgX");
	static TAutoConsoleVariable<float> TonemapExposure("r.Tonemap.Exposure", 1.0f, "Tonemap exposure applied in addition to exposure from AutoExposure pass");
	
	ToneMapPass::ToneMapPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSO();
	}

	void ToneMapPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		RGResourceName source = postprocessor->GetFinalResource();
		RGResourceName destination = postprocessor->HasFXAA() && !postprocessor->IsPathTracing() ? RG_NAME(TonemapOutput) : RG_NAME(FinalTexture);
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		BloomBlackboardData const* bloom_data = rg.GetBlackboard().TryGet<BloomBlackboardData>();

		struct ToneMapPassData
		{
			RGTextureReadOnlyId  hdr_input;
			RGTextureReadOnlyId  exposure;
			RGTextureReadOnlyId  bloom;
			RGTextureReadWriteId output;
		};

		rg.AddPass<ToneMapPassData>("Tonemap Pass",
			[=](ToneMapPassData& data, RenderGraphBuilder& builder)
			{
				if (!builder.IsTextureDeclared(destination))
				{
					RGTextureDesc destination_desc{};
					destination_desc.width = width;
					destination_desc.height = height;
					destination_desc.format = GfxFormat::R8G8B8A8_UNORM;
					builder.DeclareTexture(destination, destination_desc);
				}
				data.hdr_input = builder.ReadTexture(source, ReadAccess_NonPixelShader);

				if (builder.IsTextureDeclared(RG_NAME(Exposure))) data.exposure = builder.ReadTexture(RG_NAME(Exposure), ReadAccess_NonPixelShader);
				else data.exposure.Invalidate();

				if (builder.IsTextureDeclared(RG_NAME(Bloom))) data.bloom = builder.ReadTexture(RG_NAME(Bloom), ReadAccess_NonPixelShader);
				else data.bloom.Invalidate();

				data.output = builder.WriteTexture(destination);
				builder.SetViewport(width, height);
			},
			[=](ToneMapPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.hdr_input),
					data.exposure.IsValid() ? ctx.GetReadOnlyTexture(data.exposure) : gfxcommon::GetCommonView(GfxCommonViewType::WhiteTexture2D_SRV),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors) + 1);
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				uint32 const i = dst_descriptor.GetIndex();

				bool const bloom_enabled = data.bloom.IsValid();
				if (bloom_enabled) gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + ARRAYSIZE(src_descriptors)), ctx.GetReadOnlyTexture(data.bloom));

				struct TonemapConstants
				{
					float    tonemap_exposure;
					uint32   tonemap_operator_lut_packed;
					uint32   hdr_idx;
					uint32   exposure_idx;
					uint32   output_idx;
					int32    bloom_idx;
					uint32   lens_dirt_idx;
					uint32   bloom_params_packed;
				} constants =
				{
					.tonemap_exposure = TonemapExposure.Get(), .tonemap_operator_lut_packed = PackTwoUint16ToUint32((uint16)TonemapOperator.Get(), (uint16)tony_mc_mapface_lut_handle),
					.hdr_idx = i, .exposure_idx = i + 1, .output_idx = i + 2, .bloom_idx = -1
				};
				if (bloom_enabled)
				{
					ADRIA_ASSERT(bloom_data != nullptr);
					constants.bloom_idx = i + ARRAYSIZE(src_descriptors);
					constants.lens_dirt_idx = (uint32)lens_dirt_handle;
					constants.bloom_params_packed = PackTwoFloatsToUint32(bloom_data->bloom_intensity, bloom_data->bloom_blend_factor);
				}

				cmd_list->SetPipelineState(tonemap_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(width, 16), DivideAndRoundUp(height, 16), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		postprocessor->SetFinalResource(destination);
		input = source;
	}

	void ToneMapPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void ToneMapPass::OnSceneInitialized()
	{
		lens_dirt_handle		   = g_TextureManager.LoadTexture(paths::TexturesDir + "LensDirt.dds");
		tony_mc_mapface_lut_handle = g_TextureManager.LoadTexture(paths::TexturesDir + "tony_mc_mapface.dds");
	}

	void ToneMapPass::CreatePSO()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_Tonemap;
		tonemap_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	void ToneMapPass::GUI()
	{
		QueueGUI([&]() 
			{
				if (ImGui::TreeNode("Tone Mapping"))
				{
					ImGui::SliderFloat("Exposure", TonemapExposure.GetPtr(), 0.01f, 10.0f);
					static char const* const operators[] = { "None", "Reinhard", "Hable", "Linear", "Tony McMapface", "AgX" };
					ImGui::ListBox("Tone Map Operator", TonemapOperator.GetPtr(), operators, IM_ARRAYSIZE(operators));
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing);
	}
}