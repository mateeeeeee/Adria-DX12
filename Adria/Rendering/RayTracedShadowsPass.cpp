#include "RayTracedShadowsPass.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxShaderKey.h"
#include "Graphics/GfxStateObject.h"
#include "RenderGraph/RenderGraph.h"
namespace adria
{
	RayTracedShadowsPass::RayTracedShadowsPass(GfxDevice* gfx, uint32 width, uint32 height)
		: gfx(gfx), width(width), height(height)
	{
		is_supported = gfx->GetCapabilities().SupportsRayTracing();
		if (IsSupported())
		{
			CreateStateObject();
			ShaderManager::GetLibraryRecompiledEvent().AddMember(&RayTracedShadowsPass::OnLibraryRecompiled, *this);
		}
	}
	RayTracedShadowsPass::~RayTracedShadowsPass() = default;

	void RayTracedShadowsPass::AddPass(RenderGraph& rg, uint32 light_index)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct RayTracedShadowsPassData
		{
			RGTextureReadOnlyId depth;
		};

		rg.AddPass<RayTracedShadowsPassData>("Ray Traced Shadows Pass",
			[=](RayTracedShadowsPassData& data, RGBuilder& builder)
			{
				data.depth = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](RayTracedShadowsPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				uint32 i = gfx->AllocateDescriptorsGPU(1).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), ctx.GetReadOnlyTexture(data.depth));

				struct RayTracedShadowsConstants
				{
					uint32  depth_idx;
					uint32  light_idx;
				} constants =
				{
					.depth_idx = i,
					.light_idx = light_index
				};
				auto& table = cmd_list->SetStateObject(ray_traced_shadows_so.get());
				table.SetRayGenShader("RTS_RayGen_Hard");
				table.AddMissShader("RTS_Miss", 0);
				table.AddHitGroup("ShadowAnyHitGroup", 0);

				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->DispatchRays(width, height);

			}, RGPassType::Compute, RGPassFlags::ForceNoCull);
	}

	void RayTracedShadowsPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	bool RayTracedShadowsPass::IsSupported() const
	{
		return is_supported;
	}

	void RayTracedShadowsPass::CreateStateObject()
	{
		GfxShader const& rt_shadows_blob = GetGfxShader(LIB_Shadows);
		GfxStateObjectBuilder rt_shadows_state_object_builder(6);
		{
			D3D12_EXPORT_DESC export_descs[] =
			{
				D3D12_EXPORT_DESC{.Name = L"RTS_RayGen_Hard", .ExportToRename = L"RTS_RayGen"},
				D3D12_EXPORT_DESC{.Name = L"RTS_AnyHit", .ExportToRename = NULL},
				D3D12_EXPORT_DESC{.Name = L"RTS_Miss", .ExportToRename = NULL}
			};

			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rt_shadows_blob.GetSize();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rt_shadows_blob.GetData();
			dxil_lib_desc.NumExports = ARRAYSIZE(export_descs);
			dxil_lib_desc.pExports = export_descs;
			rt_shadows_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG rt_shadows_shader_config{};
			rt_shadows_shader_config.MaxPayloadSizeInBytes = 4;
			rt_shadows_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rt_shadows_state_object_builder.AddSubObject(rt_shadows_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = gfx->GetCommonRootSignature();
			rt_shadows_state_object_builder.AddSubObject(global_root_sig);

			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 1;
			rt_shadows_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC anyhit_group{};
			anyhit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			anyhit_group.AnyHitShaderImport = L"RTS_AnyHit";
			anyhit_group.HitGroupExport = L"ShadowAnyHitGroup";
			rt_shadows_state_object_builder.AddSubObject(anyhit_group);
		}
		ray_traced_shadows_so.reset(rt_shadows_state_object_builder.CreateStateObject(gfx));
	}

	void RayTracedShadowsPass::OnLibraryRecompiled(GfxShaderKey const& key)
	{
		if (key.GetShaderID() == LIB_Shadows) CreateStateObject();
	}
}

