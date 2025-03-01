#include "VolumetricFogManager.h"
#include "Components.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	enum class VolumetricFogType : Uint8
	{
		None,
		Raymarching,
		FogVolume
	};

	static TAutoConsoleVariable<Int>  VolumetricFogPath("r.VolumetricPath", 1, "0 - None, 1 - 2D Raymarching, 2 - Fog Volume");

	VolumetricFogManager::VolumetricFogManager(GfxDevice* gfx, entt::registry& reg, Uint32 w, Uint32 h) : reg(reg), ray_marched_volumetric_fog_pass(gfx, w, h), fog_volumes_pass(gfx, reg, w, h)
	{
		volumetric_fog_type = static_cast<VolumetricFogType>(VolumetricFogPath.Get());
		VolumetricFogPath->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) { volumetric_fog_type = static_cast<VolumetricFogType>(cvar->GetInt()); }));
	}
	VolumetricFogManager::~VolumetricFogManager() = default;

	void VolumetricFogManager::AddPass(RenderGraph& rg)
	{
		auto view = reg.view<Light>();
		Uint32 volumetric_light_count = (Uint32)std::count_if(view.begin(), view.end(), [&view](auto entity)
		{
			return view.get<Light>(entity).volumetric;
		});
		if (volumetric_light_count == 0) return;

		if (volumetric_fog_type == VolumetricFogType::FogVolume)
		{
			fog_volumes_pass.AddPasses(rg);
		}
		else if (volumetric_fog_type == VolumetricFogType::Raymarching)
		{
			ray_marched_volumetric_fog_pass.AddPass(rg);
		}
	}

	void VolumetricFogManager::GUI()
	{
		QueueGUI([this]() 
		{
		if (ImGui::TreeNode("Volumetric Settings"))
		{
			if (ImGui::Combo("Volumetric Fog Type", VolumetricFogPath.GetPtr(), "None\0 Raymarching\0Fog Volume\0", 3))
			{
				volumetric_fog_type = static_cast<VolumetricFogType>(VolumetricFogPath.Get());
			}
			switch (volumetric_fog_type)
			{
			case VolumetricFogType::Raymarching: ray_marched_volumetric_fog_pass.GUI(); break;
			case VolumetricFogType::FogVolume:	 fog_volumes_pass.GUI();				break;
			}
			ImGui::TreePop();
		}
		}, GUICommandGroup_Renderer);
	}
}