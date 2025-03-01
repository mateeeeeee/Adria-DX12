#pragma once
#include "FogVolumesPass.h"
#include "RayMarchedVolumetricFogPass.h"

namespace adria
{
	enum class VolumetricFogType : Uint8;

	class VolumetricFogManager
	{
	public:
		VolumetricFogManager(GfxDevice* gfx, entt::registry& reg, Uint32 w, Uint32 h);
		~VolumetricFogManager();

		void AddPass(RenderGraph& rg);
		void OnResize(Uint32 w, Uint32 h)
		{
			ray_marched_volumetric_fog_pass.OnResize(w, h);
			fog_volumes_pass.OnResize(w, h);
		}
		void OnSceneInitialized()
		{
			fog_volumes_pass.OnSceneInitialized();
		}
		void OnShadowTextureRendered(RGResourceName name)
		{
			ray_marched_volumetric_fog_pass.OnShadowTextureRendered(name);
		}
		void GUI();

	private:
		VolumetricFogType volumetric_fog_type;
		RayMarchedVolumetricFogPass ray_marched_volumetric_fog_pass;
		FogVolumesPass fog_volumes_pass;
	};
}

