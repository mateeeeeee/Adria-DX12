#include <format>
#include "BloomPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "PostProcessor.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"

namespace adria
{
	static TAutoConsoleVariable<Bool>  Bloom("r.Bloom", false, "Enable or Disable Bloom");
	static TAutoConsoleVariable<Float> BloomRadius("r.Bloom.Radius", 0.25f, "Controls the radius of the bloom effect");
	static TAutoConsoleVariable<Float> BloomIntensity("r.Bloom.Intensity", 1.33f, "Controls the intensity of the bloom effect");
	static TAutoConsoleVariable<Float> BloomBlendFactor("r.Bloom.BlendFactor", 0.25f, "Controls the blend factor of the bloom effect");

	BloomPass::BloomPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreatePSOs();
	}
	BloomPass::~BloomPass() = default;

	void BloomPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		Uint32 pass_count = (Uint32)std::floor(log2f((Float)std::max(width, height))) - 3;
		std::vector<RGResourceName> downsample_mips(pass_count);
		downsample_mips[0] = DownsamplePass(rg, postprocessor->GetFinalResource(), 1);
		for (Uint32 i = 1; i < pass_count; ++i)
		{
			downsample_mips[i] = DownsamplePass(rg, downsample_mips[i - 1], i + 1);
		}

		std::vector<RGResourceName> upsample_mips(pass_count);
		upsample_mips[pass_count - 1] = downsample_mips[pass_count - 1];

		for (Int32 i = pass_count - 2; i >= 0; --i)
		{
			upsample_mips[i] = UpsamplePass(rg, downsample_mips[i], upsample_mips[i + 1], i + 1);
		}

		BloomBlackboardData blackboard_data{ .bloom_intensity = BloomIntensity.Get(), .bloom_blend_factor = BloomBlendFactor.Get() };
		rg.GetBlackboard().Add<BloomBlackboardData>(std::move(blackboard_data));
	}

	void BloomPass::OnResize(Uint32 w, Uint32 h)
	{
		width = w, height = h;
	}

	Bool BloomPass::IsEnabled(PostProcessor const*) const
	{
		return Bloom.Get();
	}

	void BloomPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("Bloom", 0))
				{
					ImGui::Checkbox("Enable", Bloom.GetPtr());
					if (Bloom.Get())
					{
						ImGui::SliderFloat("Bloom Radius", BloomRadius.GetPtr(), 0.0f, 1.0f);
						ImGui::SliderFloat("Bloom Intensity", BloomIntensity.GetPtr(), 0.0f, 8.0f);
						ImGui::SliderFloat("Bloom Blend Factor", BloomBlendFactor.GetPtr(), 0.0f, 1.0f);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessing
		);

	}

	void BloomPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_BloomDownsample;
		downsample_psos = std::make_unique<GfxComputePipelineStatePermutations>(2, compute_pso_desc);
		downsample_psos->AddDefine<1>("FIRST_PASS", "1");
		downsample_psos->Finalize(gfx);

		compute_pso_desc.CS = CS_BloomUpsample;
		upsample_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

	RGResourceName BloomPass::DownsamplePass(RenderGraph& rg, RGResourceName input, Uint32 pass_idx)
	{
		Uint32 target_dim_x = std::max(1u, width >> pass_idx);
		Uint32 target_dim_y = std::max(1u, height >> pass_idx);

		RGResourceName output = RG_NAME_IDX(BloomDownsample, pass_idx);
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct BloomDownsamplePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId  input;
		};

		std::string pass_name = std::format("Bloom Downsample Pass {}", pass_idx);
		rg.AddPass<BloomDownsamplePassData>(pass_name.c_str(),
			[=](BloomDownsamplePassData& data, RenderGraphBuilder& builder)
			{
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);

				RGTextureDesc desc{};
				desc.width = target_dim_x;
				desc.height = target_dim_y;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(output, desc);
				data.output = builder.WriteTexture(output);
			},
			[=](BloomDownsamplePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.input),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct BloomDownsampleConstants
				{
					Float    dims_inv_x;
					Float    dims_inv_y;
					Uint32   source_idx;
					Uint32   target_idx;
				} constants =
				{
					.dims_inv_x = 1.0f / target_dim_x,
					.dims_inv_y = 1.0f / target_dim_y,
					.source_idx = i,
					.target_idx = i + 1
				};
				GfxPipelineState* pso = pass_idx == 1 ? downsample_psos->Get<1>() : downsample_psos->Get<0>();
				cmd_list->SetPipelineState(pso);
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(target_dim_x, 8), DivideAndRoundUp(target_dim_y, 8), 1);
			}, RGPassType::Compute, RGPassFlags::None);

		return output;
	}

	RGResourceName BloomPass::UpsamplePass(RenderGraph& rg, RGResourceName input_high, RGResourceName input_low, Uint32 pass_idx)
	{
		struct BloomUpsamplePassData
		{
			RGTextureReadWriteId output;
			RGTextureReadOnlyId  input_low;
			RGTextureReadOnlyId  input_high;
		};

		Uint32 target_dim_x = std::max(1u, width >> pass_idx);
		Uint32 target_dim_y = std::max(1u, height >> pass_idx);

		RGResourceName output = pass_idx != 1 ? RG_NAME_IDX(BloomUpsample, pass_idx) : RG_NAME(Bloom);
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		std::string pass_name = std::format("Bloom Upsample Pass {}", pass_idx);
		rg.AddPass<BloomUpsamplePassData>(pass_name.c_str(),
			[=](BloomUpsamplePassData& data, RenderGraphBuilder& builder)
			{
				data.input_high = builder.ReadTexture(input_high);
				data.input_low  = builder.ReadTexture(input_low);

				RGTextureDesc desc{};
				desc.width = target_dim_x;
				desc.height = target_dim_y;
				desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(output, desc);
				data.output = builder.WriteTexture(output);
			},
			[=](BloomUpsamplePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadOnlyTexture(data.input_low),
					ctx.GetReadOnlyTexture(data.input_high),
					ctx.GetReadWriteTexture(data.output)
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				struct BloomUpsampleConstants
				{
					Float    dims_inv_x;
					Float    dims_inv_y;
					Uint32   low_input_idx;
					Uint32   high_input_idx;
					Uint32   output_idx;
					Float    radius;
				} constants =
				{
					.dims_inv_x = 1.0f / (target_dim_x),
					.dims_inv_y = 1.0f / (target_dim_y),
					.low_input_idx = i,
					.high_input_idx = i + 1,
					.output_idx = i + 2,
					.radius = BloomRadius.Get()
				};

				cmd_list->SetPipelineState(upsample_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(target_dim_x, 8), DivideAndRoundUp(target_dim_y, 8), 1);
			}, RGPassType::Compute, RGPassFlags::None);


		return output;
	}

}

