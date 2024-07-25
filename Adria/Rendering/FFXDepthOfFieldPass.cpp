#include "FFXDepthOfFieldPass.h"
#include "FidelityFXUtils.h"
#include "BlackboardData.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Logging/Logger.h"
#include "Math/Constants.h"
#include "Core/ConsoleVariable.h"

namespace adria
{


	namespace cvars
	{
		static ConsoleVariable dof_aperture("ffx.dof.aperture", 0.01f);
		static ConsoleVariable dof_focus_distance("ffx.dof.focus.distance", 400.0f);
		static ConsoleVariable dof_sensor_size("ffx.dof.sensor.size", 0.02f);
		static ConsoleVariable dof_coc_limit("ffx.dof.coc.limit", 0.01f);
		static ConsoleVariable dof_quality("ffx.dof.quality", 10);
		static ConsoleVariable dof_ring_merging("ffx.dof.ring.merging", false);
	}

	FFXDepthOfFieldPass::FFXDepthOfFieldPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h), ffx_interface(nullptr)
	{
		if (!gfx->GetCapabilities().SupportsShaderModel(SM_6_6)) return;

		sprintf(name_version, "FFX DoF %d.%d.%d", FFX_DOF_VERSION_MAJOR, FFX_DOF_VERSION_MINOR, FFX_DOF_VERSION_PATCH);
		ffx_interface = CreateFfxInterface(gfx, FFX_DOF_CONTEXT_COUNT);
		dof_context_desc.backendInterface = *ffx_interface;
		CreateContext();
		SetCVarCallbacks();
	}

	FFXDepthOfFieldPass::~FFXDepthOfFieldPass()
	{
		DestroyContext();
		DestroyFfxInterface(ffx_interface);
	}

	RGResourceName FFXDepthOfFieldPass::AddPass(RenderGraph& rg, RGResourceName input)
	{
		struct FFXDoFPassData
		{
			RGTextureReadOnlyId input;
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId output;
		};

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.AddPass<FFXDoFPassData>(name_version,
			[=](FFXDoFPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc ffx_dof_desc = builder.GetTextureDesc(input);
				builder.DeclareTexture(RG_RES_NAME(FFXDoFOutput), ffx_dof_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(FFXDoFOutput));
				data.input = builder.ReadTexture(input, ReadAccess_NonPixelShader);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](FFXDoFPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture& input_texture = ctx.GetTexture(*data.input);
				GfxTexture& depth_texture = ctx.GetTexture(*data.depth);
				GfxTexture& output_texture = ctx.GetTexture(*data.output);

				float fovx = std::min<float>(frame_data.camera_fov * frame_data.camera_aspect_ratio, pi_div_2<float>);
				float conversion = 0.5f * (float)dof_context_desc.resolution.width / sensor_size;
				float focal_length = sensor_size / (2.0f * std::tanf(fovx * 0.5f));

				FfxDofDispatchDescription ffx_dof_dispatch_desc{};
				ffx_dof_dispatch_desc.commandList = ffxGetCommandListDX12(cmd_list->GetNative());
				ffx_dof_dispatch_desc.color	   = GetFfxResource(input_texture);
				ffx_dof_dispatch_desc.depth	   = GetFfxResource(depth_texture);
				ffx_dof_dispatch_desc.output   = GetFfxResource(output_texture, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

				Matrix proj(frame_data.camera_proj);
				ffx_dof_dispatch_desc.cocScale = ffxDofCalculateCocScale(aperture, -focus_dist, focal_length, conversion, proj._33, proj._43, proj._34);
				ffx_dof_dispatch_desc.cocBias = ffxDofCalculateCocBias(aperture, -focus_dist, focal_length, conversion, proj._33, proj._43, proj._34);

				FfxErrorCode error_code = ffxDofContextDispatch(&dof_context, &ffx_dof_dispatch_desc);
				ADRIA_ASSERT(error_code == FFX_OK);

				cmd_list->ResetState();
			}, RGPassType::Compute);

		GUI_Command([&]()
			{
				if (ImGui::TreeNodeEx(name_version, ImGuiTreeNodeFlags_None))
				{
					ImGui::SliderFloat("Aperture", &aperture, 0.0f, 0.1f, "%.2f");
					ImGui::SliderFloat("Sensor Size", &sensor_size, 0.0f, 0.1f, "%.2f");
					ImGui::SliderFloat("Focus Distance", &focus_dist, frame_data.camera_near, frame_data.camera_far, "%.2f");

					bool recreate_context = false;

					recreate_context |= ImGui::SliderInt("Quality", &quality, 1, 50);
					recreate_context |= ImGui::SliderFloat("Blur Size Limit", &coc_limit, 0.0f, 1.0f);
					recreate_context |= ImGui::Checkbox("Enable Kernel Ring Merging", &enable_ring_merge);

					if (recreate_context)
					{
						DestroyContext();
						CreateContext();
					}

					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessor);

		return RG_RES_NAME(FFXDoFOutput);
	}

	void FFXDepthOfFieldPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		DestroyContext();
		CreateContext();
	}

	void FFXDepthOfFieldPass::SetCVarCallbacks()
	{
		ADRIA_CVAR_CALLBACK(dof_aperture, (float v) { aperture = v; });
		ADRIA_CVAR_CALLBACK(dof_sensor_size, (float v) { sensor_size = v; });
		ADRIA_CVAR_CALLBACK(dof_focus_distance, (float v) { focus_dist = v; });
		ADRIA_CVAR_CALLBACK(dof_quality, (int v) {
			quality = v; 
			DestroyContext();
			CreateContext();
		});
		ADRIA_CVAR_CALLBACK(dof_coc_limit, (float v) {
			coc_limit = v;
			DestroyContext();
			CreateContext();
		});
		ADRIA_CVAR_CALLBACK(dof_ring_merging, (bool v) {
			enable_ring_merge = v;
			DestroyContext();
			CreateContext();
		});
	}

	void FFXDepthOfFieldPass::CreateContext()
	{
		dof_context_desc.flags = FFX_DOF_REVERSE_DEPTH;
		if (!enable_ring_merge) dof_context_desc.flags |= FFX_DOF_DISABLE_RING_MERGE;
		
		dof_context_desc.resolution.width = width;
		dof_context_desc.resolution.height = height;
		dof_context_desc.quality = static_cast<uint32>(quality);
		dof_context_desc.cocLimitFactor = coc_limit;

		FfxErrorCode error_code = ffxDofContextCreate(&dof_context, &dof_context_desc);
		ADRIA_ASSERT(error_code == FFX_OK);
	}

	void FFXDepthOfFieldPass::DestroyContext()
	{
		gfx->WaitForGPU();
		ffxDofContextDestroy(&dof_context);
	}

}

