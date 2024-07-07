#pragma once
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;

	class VolumetricFogPass
	{
		struct FogVolume
		{

		};
		struct FogVolumeGPU
		{
			
		};
	public:

		VolumetricFogPass(GfxDevice* gfx, uint32 w, uint32 h);
		void AddPasses(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			CreateVoxelTexture();
		}

		void OnSceneInitialized();

	private:
		GfxDevice* gfx;
		uint32 width, height;

		std::unique_ptr<GfxTexture> voxel_grid_history;
		GfxDescriptor voxel_grid_history_srv;
		uint32 voxel_grid_history_idx;

		std::vector<FogVolumeGPU> fog_volumes;
		std::unique_ptr<GfxBuffer> fog_volumes_buffer;
		GfxDescriptor fog_volumes_buffer_srv;
		uint32 fog_volumes_buffer_idx;

	private:

		void CreateVoxelTexture();

		void AddLightInjectionPass(RenderGraph& rendergraph);
		void AddScatteringAccumulationPass(RenderGraph& rendergraph);
	};
}