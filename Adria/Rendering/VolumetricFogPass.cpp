#include "VolumetricFogPass.h"
#include "BlackboardData.h"
#include "PSOCache.h" 
#include "Components.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxBuffer.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/Paths.h"

namespace adria
{
	static constexpr uint32 VOXEL_TEXEL_SIZE_X = 8;
	static constexpr uint32 VOXEL_TEXEL_SIZE_Y = 8;
	static constexpr uint32 VOXEL_GRID_SIZE_Z  = 128;


	VolumetricFogPass::VolumetricFogPass(GfxDevice* gfx, entt::registry& reg, uint32 w, uint32 h) : gfx(gfx), reg(reg), width(w), height(h)
	{
		CreateVoxelTexture();
	}

	void VolumetricFogPass::AddPasses(RenderGraph& rg)
	{
		GfxDescriptor ddgi_volume_buffer_srv_gpu = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, ddgi_volume_buffer_srv_gpu, fog_volume_buffer_srv);
		fog_volume_buffer_idx = ddgi_volume_buffer_srv_gpu.GetIndex();

		AddLightInjectionPass(rg);
		AddScatteringAccumulationPass(rg);

		GUI_RunCommand([&]()
			{
				if (fog_volumes.empty()) return;

				FogVolume& fog_volume = fog_volumes[0];
				if (ImGui::TreeNode("Volumetric Fog"))
				{
					ImGui::Checkbox("Temporal Accumulation", &temporal_accumulation);

					bool update_fog_volume_buffer = false;
					update_fog_volume_buffer |= ImGui::SliderFloat("Density Base", &fog_volume.density_base, 0.0f, 1.0f);
					update_fog_volume_buffer |= ImGui::SliderFloat("Density Change", &fog_volume.density_change, 0.0f, 1.0f);
					Vector3 fog_color = fog_volume.color.ToVector3();
					update_fog_volume_buffer |= ImGui::ColorEdit3("Fog Color", (float*)&fog_color);
					fog_volume.color = Color(fog_color);

					if (update_fog_volume_buffer)
					{
						CreateFogVolumeBuffer();
					}
					
					ImGui::TreePop();
				}
			}, GUICommandGroup_Renderer);
	}

	void VolumetricFogPass::OnSceneInitialized()
	{
		std::string blue_noise_base_path = paths::TexturesDir() + "BlueNoise/";
		for (uint32 i = 0; i < BLUE_NOISE_TEXTURE_COUNT; ++i)
		{
			std::string blue_noise_texture_path = blue_noise_base_path + "LDR_LLL1_" + std::to_string(i) + ".png";
			blue_noise_handles[i] = g_TextureManager.LoadTexture(blue_noise_texture_path);
		}

		BoundingBox scene_bounding_box;
		for (auto mesh_entity : reg.view<Mesh>())
		{
			Mesh& mesh = reg.get<Mesh>(mesh_entity);
			for (auto const& instance : mesh.instances)
			{
				SubMeshGPU& submesh = mesh.submeshes[instance.submesh_index];
				BoundingBox instance_bounding_box;
				submesh.bounding_box.Transform(instance_bounding_box, instance.world_transform);
				BoundingBox::CreateMerged(scene_bounding_box, scene_bounding_box, instance_bounding_box);
			}
		}

		FogVolume& fog_volume = fog_volumes.emplace_back();
		fog_volume.volume = scene_bounding_box;
		fog_volume.color = Color(1, 1, 1);
		fog_volume.density_base = 0.0f;
		fog_volume.density_change = 0.05f;

		CreateFogVolumeBuffer();
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

	void VolumetricFogPass::CreateFogVolumeBuffer()
	{
		if (!fog_volume_buffer || fog_volume_buffer->GetCount() < fog_volumes.size())
		{
			fog_volume_buffer = gfx->CreateBuffer(StructuredBufferDesc<FogVolumeGPU>(fog_volumes.size(), false, true));
			fog_volume_buffer_srv = gfx->CreateBufferSRV(fog_volume_buffer.get());
		}

		std::vector<FogVolumeGPU> gpu_fog_volumes;
		for (auto const& fog_volume : fog_volumes)
		{
			FogVolumeGPU fog_volume_gpu =
			{
				.center = fog_volume.volume.Center,
				.extents = fog_volume.volume.Extents,
				.color = fog_volume.color.ToVector3(),
				.density_base = fog_volume.density_base,
				.density_change = fog_volume.density_change
			};
			gpu_fog_volumes.push_back(std::move(fog_volume_gpu));
		}
		fog_volume_buffer->Update(gpu_fog_volumes.data(), gpu_fog_volumes.size() * sizeof(FogVolumeGPU));
	}

	void VolumetricFogPass::AddLightInjectionPass(RenderGraph& rg)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.ImportTexture(RG_RES_NAME(FogVoxelGridHistory), voxel_grid_history.get());

		struct LightInjectionPassData
		{
			RGTextureReadWriteId voxel_grid;
			RGTextureReadOnlyId  voxel_grid_history;
			RGBufferReadOnlyId   fog_volume_buffer;
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
				
				uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadWriteTexture(data.voxel_grid));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadOnlyTexture(data.voxel_grid_history));
				
				struct LightInjectionConstants
				{
					Vector3u voxel_grid_dimensions;
					uint32 fog_volumes_count;
					uint32 fog_volume_buffer_idx;
					uint32 voxel_grid_idx;
					uint32 voxel_grid_history_idx;
					uint32 blue_noise_idx;
				} constants =
				{
					.voxel_grid_dimensions = Vector3u(voxel_grid_history->GetWidth(), voxel_grid_history->GetHeight(), voxel_grid_history->GetDepth()),
					.fog_volumes_count = fog_volume_buffer->GetCount(),
					.fog_volume_buffer_idx = fog_volume_buffer_idx,
					.voxel_grid_idx = i,
					.voxel_grid_history_idx = i + 1,
					.blue_noise_idx = (uint32)blue_noise_handles[temporal_accumulation ? gfx->GetFrameIndex() % BLUE_NOISE_TEXTURE_COUNT : 0]
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

