#include "ReflectionPassGroup.h"
#include "RayTracedReflectionsPass.h"
#include "SSRPass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"


namespace adria
{
	static TAutoConsoleVariable<int> Reflection("r.Reflections", 0, "0 - No Reflections, 1 - SSR, 2 - RTR");

	enum class ReflectionType : uint8
	{
		None,
		SSR,
		RTR,
		Count
	};

	ReflectionPassGroup::ReflectionPassGroup(GfxDevice* gfx, uint32 width, uint32 height) : reflection_type(ReflectionType::None)
	{
		post_effect_idx = static_cast<uint32>(reflection_type);
		Reflection->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar)
			{
				reflection_type = static_cast<ReflectionType>(cvar->GetInt());
				post_effect_idx = static_cast<uint32>(reflection_type);
			}));

		using enum ReflectionType;
		post_effects.resize((uint32)Count);
		post_effects[(uint32)None] = std::make_unique<EmptyPostEffect>();
		post_effects[(uint32)SSR]  = std::make_unique<SSRPass>(gfx, width, height);
		post_effects[(uint32)RTR]  = std::make_unique<RayTracedReflectionsPass>(gfx, width, height);
		is_rtr_supported = post_effects[(uint32)RTR]->IsSupported();
	}

	void ReflectionPassGroup::GroupGUI()
	{
		QueueGUI([&]()
			{
				static int current_reflection_type = (int)reflection_type;
				if (ImGui::Combo("Reflections", &current_reflection_type, "None\0SSR\0RTR\0", 3))
				{
					if (!is_rtr_supported && current_reflection_type == 2) current_reflection_type = 1;
					Reflection->Set(current_reflection_type);
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Reflection);
	}

}
