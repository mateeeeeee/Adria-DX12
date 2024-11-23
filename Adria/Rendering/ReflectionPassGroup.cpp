#include "ReflectionPassGroup.h"
#include "RayTracedReflectionsPass.h"
#include "SSRPass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"


namespace adria
{
	enum ReflectionType : Uint8
	{
		ReflectionType_None,
		ReflectionType_SSR,
		ReflectionType_RTR,
		ReflectionType_Count
	};

	static TAutoConsoleVariable<int> Reflection("r.Reflections", ReflectionType_SSR, "0 - No Reflections, 1 - SSR, 2 - RTR");

	ReflectionPassGroup::ReflectionPassGroup(GfxDevice* gfx, Uint32 width, Uint32 height) : reflection_type(ReflectionType_SSR)
	{
		post_effect_idx = static_cast<Uint32>(reflection_type);
		Reflection->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar)
			{
				reflection_type = static_cast<ReflectionType>(cvar->GetInt());
				post_effect_idx = static_cast<Uint32>(reflection_type);
			}));

		post_effects.resize(ReflectionType_Count);
		post_effects[ReflectionType_None] = std::make_unique<EmptyPostEffect>();
		post_effects[ReflectionType_SSR]  = std::make_unique<SSRPass>(gfx, width, height);
		post_effects[ReflectionType_RTR]  = std::make_unique<RayTracedReflectionsPass>(gfx, width, height);
		is_rtr_supported = post_effects[ReflectionType_RTR]->IsSupported();
	}

	void ReflectionPassGroup::GroupGUI()
	{
		QueueGUI([&]()
			{
				static Sint current_reflection_type = (Sint)reflection_type;
				if (ImGui::Combo("Reflections", &current_reflection_type, "None\0SSR\0RTR\0", 3))
				{
					if (!is_rtr_supported && current_reflection_type == 2) current_reflection_type = 1;
					Reflection->Set(current_reflection_type);
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Reflection);
	}

}
