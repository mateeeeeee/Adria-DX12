#include "VolumetricFogManager.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"

namespace adria
{
	static TAutoConsoleVariable<Int>  VolumetricFogPath("r.VolumetricPath", 1, "0 - None, 1 - 2D Raymarching, 2 - Fog Volume");

	VolumetricFogManager::VolumetricFogManager(GfxDevice* gfx, entt::registry& reg, Uint32 w, Uint32 h) : volumetric_lighting_pass(gfx, w, h), volumetric_fog_pass(gfx, reg, w, h)
	{
		volumetric_fog_type = static_cast<VolumetricFogType>(VolumetricFogPath.Get());
		VolumetricFogPath->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) { volumetric_fog_type = static_cast<VolumetricFogType>(cvar->GetInt()); }));
	}

	VolumetricFogManager::~VolumetricFogManager()
	{
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
			case VolumetricFogType::Raymarching: volumetric_lighting_pass.GUI(); break;
			case VolumetricFogType::FogVolume:	 volumetric_fog_pass.GUI();		 break;
			}
			ImGui::TreePop();
		}
		}, GUICommandGroup_Renderer);
	}
}