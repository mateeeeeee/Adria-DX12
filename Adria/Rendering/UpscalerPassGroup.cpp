#include "UpscalerPassGroup.h"
#include "FSR2Pass.h"
#include "FSR3Pass.h"
#include "XeSSPass.h"
#include "DLSS3Pass.h"
#include "Core/ConsoleManager.h"
#include "Editor/GUICommand.h"
#include "Logging/Logger.h"

namespace adria
{
	static TAutoConsoleVariable<int>  Upscaler("r.Upscaler", 0, "0 - No Upscaler, 1 - FSR2, 2 - FSR3, 3 - XeSS, 4 - DLSS3");
	
	enum class UpscalerType : uint8
	{
		None,
		FSR2,
		FSR3,
		XeSS,
		DLSS3,
		Count
	};

	UpscalerPassGroup::UpscalerPassGroup(GfxDevice* gfx, uint32 width, uint32 height) : upscaler_type(UpscalerType::None), display_width(width), display_height(height)
	{
		post_effect_idx = static_cast<uint32>(upscaler_type);
		Upscaler->AddOnChanged(ConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* cvar) 
			{ 
				upscaler_type = static_cast<UpscalerType>(cvar->GetInt());
				post_effect_idx = static_cast<uint32>(upscaler_type);
			}));

		using enum UpscalerType; 
		post_effects.resize((uint32)Count);
		post_effects[(uint32)None]  = std::make_unique<EmptyUpscalerPass>();
		post_effects[(uint32)FSR2]  = std::make_unique<FSR2Pass>(gfx, width, height);
		post_effects[(uint32)FSR3]  = std::make_unique<FSR3Pass>(gfx, width, height);
		post_effects[(uint32)XeSS]  = std::make_unique<XeSSPass>(gfx, width, height);
		post_effects[(uint32)DLSS3] = std::make_unique<DLSS3Pass>(gfx, width, height);
	}

	void UpscalerPassGroup::OnResize(uint32 w, uint32 h)
	{
		display_width = w, display_height = h;
		if (upscaler_type != UpscalerType::None)
		{
			post_effects[(uint32)upscaler_type]->OnResize(display_width, display_height);
		}
		else
		{
			upscaler_disabled_event.Broadcast(display_width, display_height);
		}
	}

	void UpscalerPassGroup::GroupGUI()
	{
		QueueGUI([&]()
			{
				static int current_upscaler = (int)upscaler_type;
				if (ImGui::Combo("Upscaler", &current_upscaler, "None\0FSR2\0FSR3\0XeSS\0DLSS3\0", 5))
				{
					upscaler_type = static_cast<UpscalerType>(current_upscaler);
					if (upscaler_type == UpscalerType::DLSS3 && !post_effects[(uint32)UpscalerType::DLSS3]->IsSupported())
					{
						upscaler_type = UpscalerType::None;
						current_upscaler = 0;
						ADRIA_LOG(WARNING, "DLSS3 is not supported on this device!");
					}
					Upscaler->Set(current_upscaler);

					if (upscaler_type != UpscalerType::None)
					{
						post_effects[(uint32)upscaler_type]->OnResize(display_width, display_height);
					}
					else
					{
						upscaler_disabled_event.Broadcast(display_width, display_height);
					}
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Upscaler);
	}
}

