#pragma once
#include "FogVolumesPass.h"
#include "RayMarchedVolumetricFog.h"

namespace adria
{
	class VolumetricFogManager
	{
		enum class VolumetricFogType : Uint8
		{
			None,
			Raymarching,
			FogVolume
		};
	public:
		VolumetricFogManager(GfxDevice* gfx, entt::registry& reg, Uint32 w, Uint32 h);
		~VolumetricFogManager();

		void AddPass(RenderGraph& rg)
		{
			if (volumetric_fog_type == VolumetricFogType::FogVolume)
			{
				volumetric_fog_pass.AddPasses(rg);
			}
			else if (volumetric_fog_type == VolumetricFogType::Raymarching)
			{
				volumetric_lighting_pass.AddPass(rg);
			}
		}
		void OnResize(Uint32 w, Uint32 h)
		{
			volumetric_lighting_pass.OnResize(w, h);
			volumetric_fog_pass.OnResize(w, h);
		}
		void OnSceneInitialized()
		{
			volumetric_fog_pass.OnSceneInitialized();
		}
		void OnShadowTextureRendered(RGResourceName name)
		{
			volumetric_lighting_pass.OnShadowTextureRendered(name);
		}
		void GUI();

	private:
		VolumetricFogType volumetric_fog_type;
		RayMarchedVolumetricFog volumetric_lighting_pass;
		FogVolumesPass volumetric_fog_pass;
	};
}

