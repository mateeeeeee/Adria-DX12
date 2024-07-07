#include "VolumetricFogPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "Graphics/GfxTexture.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{
	static constexpr uint32 VOXEL_TEXEL_SIZE_X = 8;
	static constexpr uint32 VOXEL_TEXEL_SIZE_Y = 8;
	static constexpr uint32 VOXEL_GRID_SIZE_Z  = 128;


	VolumetricFogPass::VolumetricFogPass(GfxDevice* gfx, uint32 w, uint32 h) : gfx(gfx), width(w), height(h)
	{
		CreateVoxelTexture();
	}

	void VolumetricFogPass::AddPasses(RenderGraph& rg)
	{
		AddLightInjectionPass(rg);
		AddScatteringAccumulationPass(rg);
	}

	void VolumetricFogPass::OnSceneInitialized()
	{
		//create and upload noise texture
	}

	void VolumetricFogPass::CreateVoxelTexture()
	{
		uint32 const voxel_grid_width = DivideAndRoundUp(width, VOXEL_TEXEL_SIZE_X);
		uint32 const voxel_grid_height = DivideAndRoundUp(height, VOXEL_TEXEL_SIZE_Y);
		if (voxel_grid_history && voxel_grid_history->GetWidth() == voxel_grid_width &&
			voxel_grid_history->GetHeight() == voxel_grid_height)
		{
			return;
		}

		GfxTextureDesc voxel_desc{};
		voxel_desc.type = GfxTextureType_3D;
		voxel_desc.width = voxel_grid_width;
		voxel_desc.height = voxel_grid_height;
		voxel_desc.depth = VOXEL_GRID_SIZE_Z;
		voxel_desc.format = GfxFormat::R16G16B16A16_FLOAT;
		voxel_desc.bind_flags = GfxBindFlag::ShaderResource;
		voxel_desc.initial_state = GfxResourceState::CopyDst;
		voxel_grid_history = gfx->CreateTexture(voxel_desc);
		voxel_grid_history->SetName("Fog Voxel Grid History");
		voxel_grid_history_srv = gfx->CreateTextureSRV(voxel_grid_history.get());
	}

	void VolumetricFogPass::AddLightInjectionPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();


		rg.ImportTexture(RG_RES_NAME(FogVoxelGridHistory), voxel_grid_history.get());

		struct LightInjectionPassData
		{
			RGTextureReadWriteId voxel_grid;
			RGTextureReadOnlyId voxel_grid_history;
		};

		rg.AddPass<LightInjectionPassData>("Volumetric Fog Light Injection Pass",
			[=](LightInjectionPassData& data, RenderGraphBuilder& builder)
			{
				uint32 const voxel_grid_width = DivideAndRoundUp(width, VOXEL_TEXEL_SIZE_X);
				uint32 const voxel_grid_height = DivideAndRoundUp(height, VOXEL_TEXEL_SIZE_Y);

				RGTextureDesc voxel_desc{};
				voxel_desc.type = GfxTextureType_3D;
				voxel_desc.width = voxel_grid_width;
				voxel_desc.height = voxel_grid_height;
				voxel_desc.depth = VOXEL_GRID_SIZE_Z;
				voxel_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(FogVoxelGrid), voxel_desc);

				data.voxel_grid = builder.WriteTexture(RG_RES_NAME(FogVoxelGrid));
				data.voxel_grid_history = builder.ReadTexture(RG_RES_NAME(FogVoxelGridHistory));
			},
			[=](LightInjectionPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				//
				//uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
				////gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadOnlyTexture(data.ldr));
				//
				struct LightInjectionConstants
				{
				} constants =
				{
					
				};
				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::VolumetricFog_LightInjection));
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(voxel_grid_history->GetWidth(), 8), 
								   DivideAndRoundUp(voxel_grid_history->GetHeight(), 8),
								   DivideAndRoundUp(voxel_grid_history->GetDepth(), 8));
			}, RGPassType::Compute, RGPassFlags::None);
	}

	void VolumetricFogPass::AddScatteringAccumulationPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		struct ScatteringAccumulationPassData
		{
		};

		rg.AddPass<ScatteringAccumulationPassData>("Volumetric Fog Scattering Accumulation Pass",
			[=](ScatteringAccumulationPassData& data, RenderGraphBuilder& builder)
			{
				
			},
			[=](ScatteringAccumulationPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				
			}, RGPassType::Compute, RGPassFlags::None);
	}

}

