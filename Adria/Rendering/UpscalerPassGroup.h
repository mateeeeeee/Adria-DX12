#pragma once
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"

namespace adria
{
	enum class UpscalerType : uint8;
	class GfxDevice;

	using RenderResolutionChangedDelegate = Delegate<void(uint32, uint32)>;

	class UpscalerPassGroup final : public TPostEffectGroup<UpscalerPass>
	{
		DECLARE_EVENT(UpscalerDisabledEvent, UpscalerPassGroup, uint32, uint32)
	public:
		UpscalerPassGroup(GfxDevice* gfx, uint32 width, uint32 height);
		virtual void OnResize(uint32 w, uint32 h) override;

		void AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate delegate)
		{
			for (auto& post_effect : post_effects) post_effect->GetRenderResolutionChangedEvent().Add(delegate);
			upscaler_disabled_event.Add(delegate);
		}

	private:
		UpscalerType upscaler_type;
		UpscalerDisabledEvent upscaler_disabled_event;
		uint32 display_width, display_height;

	private:
		virtual void GroupGUI() override;
	};
}

