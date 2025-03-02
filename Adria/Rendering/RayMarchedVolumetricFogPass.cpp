#include "RayMarchedVolumetricFogPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h" 
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/ConsoleManager.h"

using namespace DirectX;

namespace adria
{
	enum RayMarchedVolumetricFogResolution : Int
	{
		RayMarchedVolumetricFogResolution_Full = 0,
		RayMarchedVolumetricFogResolution_Half = 1,
		RayMarchedVolumetricFogResolution_Quarter = 2
	};

	static TAutoConsoleVariable<Int>  RayMarchedVolumetricFogRes("r.VolumetricFog.RayMarching.Resolution", RayMarchedVolumetricFogResolution_Full, "Specifies in what resolution is ray marched volumetric fog computed: 0 - Full, 1 - Half, 2 - Quarter");
	static TAutoConsoleVariable<Int>  RayMarchedVolumetricFogSampleCount("r.VolumetricFog.RayMarching.SampleCount", 16, "How many samples should ray marched volumetric fog use in ray march");
	static TAutoConsoleVariable<Bool> RayMarchedVolumetricFogUsePCF("r.VolumetricFog.RayMarching.UsePCF", false, "Should the ray marched volumetric fog use PCF when calculating shadow factors");

	RayMarchedVolumetricFogPass::RayMarchedVolumetricFogPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), width(w), height(h), copy_to_texture_pass(gfx, w, h)
	{
		CreatePSOs();
	}

	RayMarchedVolumetricFogPass::~RayMarchedVolumetricFogPass()
	{
	}

	void RayMarchedVolumetricFogPass::AddPass(RenderGraph& rendergraph)
	{
		struct LightingPassData
		{
			RGTextureReadOnlyId  depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();
		Int const resolution = RayMarchedVolumetricFogRes.Get();
		rendergraph.AddPass<LightingPassData>("Ray Marched Volumetric Fog Pass",
			[=](LightingPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc volumetric_output_desc{};
				volumetric_output_desc.width = width >> resolution;
				volumetric_output_desc.height = height >> resolution;
				volumetric_output_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_NAME(VolumetricLightOutput), volumetric_output_desc);

				data.output = builder.WriteTexture(RG_NAME(VolumetricLightOutput));
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);

				for (auto& shadow_texture : shadow_textures) std::ignore = builder.ReadTexture(shadow_texture);
			},
			[=](LightingPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				
				GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.depth),
												context.GetReadWriteTexture(data.output) };
				GfxDescriptor dst_handle = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_handles));
				gfx->CopyDescriptors(dst_handle, src_handles);
				Uint32 i = dst_handle.GetIndex();

				
				struct VolumetricLightingConstants
				{
					Uint32 depth_idx;
					Uint32 output_idx;
					Uint32 resolution_scale;
					Uint32 sample_count;
				} constants =
				{
					.depth_idx = i, .output_idx = i + 1, .resolution_scale = (Uint32)resolution, .sample_count = (Uint32)RayMarchedVolumetricFogSampleCount.Get()
				};

				Bool const use_pcf = RayMarchedVolumetricFogUsePCF.Get();
				cmd_list->SetPipelineState(use_pcf ? volumetric_lighting_pso_use_pcf.get() : volumetric_lighting_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp((width >> resolution), 16), DivideAndRoundUp((height >> resolution), 16), 1);
			}, RGPassType::Compute);

		copy_to_texture_pass.AddPass(rendergraph, RG_NAME(HDR_RenderTarget), RG_NAME(VolumetricLightOutput), BlendMode::AdditiveBlend);

		shadow_textures.clear();
	}

	void RayMarchedVolumetricFogPass::GUI()
	{
		if (ImGui::TreeNodeEx("Ray Marched Volumetric Fog ", ImGuiTreeNodeFlags_None))
		{
			if (ImGui::Combo("Resolution", RayMarchedVolumetricFogRes.GetPtr(), "Full\0Half\0Quarter\0", 3))
			{
				OnResize(width, height);
			}
			ImGui::SliderInt("Sample Count", RayMarchedVolumetricFogSampleCount.GetPtr(), 1, 64);
			ImGui::Checkbox("Use PCF", RayMarchedVolumetricFogUsePCF.GetPtr());
			ImGui::TreePop();
			ImGui::Separator();
		}
	}

	void RayMarchedVolumetricFogPass::CreatePSOs()
	{
		GfxShaderKey shader_key(CS_VolumetricLighting);

		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = shader_key;
		volumetric_lighting_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		shader_key.AddDefine("USE_PCF", "1");
		compute_pso_desc.CS = shader_key;
		volumetric_lighting_pso_use_pcf = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}




