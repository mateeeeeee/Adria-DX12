#include "DepthOfFieldPassGroup.h"
#include "SimpleDepthOfFieldPass.h"
#include "FFXDepthOfFieldPass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"

namespace adria
{
	static TAutoConsoleVariable<int> DepthOfField("r.DepthOfField", 0, "0 - No Depth of Field, 1 - Simple, 2 - FFX");

	enum class DepthOfFieldType : uint8
	{
		None,
		Simple,
		FFX,
		Count
	};

	DepthOfFieldPassGroup::DepthOfFieldPassGroup(GfxDevice* gfx, uint32 width, uint32 height) : depth_of_field_type(DepthOfFieldType::None)
	{
		post_effect_idx = static_cast<uint32>(depth_of_field_type);
		DepthOfField->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) 
		{ 
			depth_of_field_type = static_cast<DepthOfFieldType>(cvar->GetInt()); 
			post_effect_idx = static_cast<uint32>(depth_of_field_type);
		}));
		using enum DepthOfFieldType;
		post_effects.resize((uint32)Count);
		post_effects[(uint32)None] = std::make_unique<EmptyPostEffect>();
		post_effects[(uint32)Simple] = std::make_unique<SimpleDepthOfFieldPass>(gfx, width, height);
		post_effects[(uint32)FFX] = std::make_unique<FFXDepthOfFieldPass>(gfx, width, height);
	}

	bool DepthOfFieldPassGroup::IsEnabled(PostProcessor const*) const
	{
		return depth_of_field_type != DepthOfFieldType::None;
	}

	void DepthOfFieldPassGroup::GroupGUI()
	{
		GUI_Command([&]()
			{
				if (ImGui::TreeNodeEx("Depth of Field", 0))
				{
					static int current_dof_type = (int)depth_of_field_type;
					if (ImGui::Combo("Depth of Field", &current_dof_type, "None\0Simple\0FFX\0", 3))
					{
						depth_of_field_type = static_cast<DepthOfFieldType>(current_dof_type);
						DepthOfField->Set(current_dof_type);
					}
					ImGui::TreePop();
					ImGui::Separator();
				}
			}, GUICommandGroup_PostProcessor
		);
	}

}

