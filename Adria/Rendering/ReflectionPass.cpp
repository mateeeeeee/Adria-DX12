#include "ReflectionPass.h"
#include "RayTracedReflectionsPass.h"
#include "SSRPass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"


namespace adria
{
	static TAutoConsoleVariable<int> cvar_reflections("r.Reflections", 0, "0 - No Reflections, 1 - SSR, 2 - RTR");

	enum class Reflections : uint8
	{
		None,
		SSR,
		RTR,
		Count
	};

	ReflectionPass::ReflectionPass(GfxDevice* gfx, uint32 width, uint32 height) : reflections(Reflections::None)
	{
		post_effect_idx = static_cast<uint32>(reflections);
		cvar_reflections->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar)
			{
				reflections = static_cast<Reflections>(cvar->GetInt());
				post_effect_idx = static_cast<uint32>(reflections);
			}));

		using enum Reflections;
		post_effects.resize((uint32)Count);
		post_effects[(uint32)None] = std::make_unique<EmptyPostEffect>();
		post_effects[(uint32)SSR] = std::make_unique<SSRPass>(gfx, width, height);
		post_effects[(uint32)RTR] = std::make_unique<RayTracedReflectionsPass>(gfx, width, height);
		is_rtr_supported = post_effects[(uint32)RTR]->IsSupported();
	}

	void ReflectionPass::GroupGUI()
	{
		GUI_Command([&]()
			{
				if (ImGui::TreeNodeEx("Reflections", 0))
				{
					static int current_reflection_type = (int)reflections;
					if (ImGui::Combo("Reflections", &current_reflection_type, "None\0SSR\0RTR\0", 3))
					{
						if (!is_rtr_supported && current_reflection_type == 2) current_reflection_type = 1;
						cvar_reflections->Set(current_reflection_type);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessor
		);
	}

}
