#pragma once
#include "UpscalerPass.h"
#include "Utilities/Delegate.h"

namespace adria
{
	enum class UpscalerType : Uint8;
	class GfxDevice;

	using RenderResolutionChangedDelegate = Delegate<void(Uint32, Uint32)>;

	class UpscalerPassGroup final : public TPostEffectGroup<UpscalerPass>
	{
		DECLARE_EVENT(UpscalerDisabledEvent, UpscalerPassGroup, Uint32, Uint32)
	public:
		UpscalerPassGroup(GfxDevice* gfx, Uint32 width, Uint32 height);
		virtual void OnResize(Uint32 w, Uint32 h) override;

		void AddRenderResolutionChangedCallback(RenderResolutionChangedDelegate delegate)
		{
			for (auto& post_effect : post_effects) post_effect->GetRenderResolutionChangedEvent().Add(delegate);
			upscaler_disabled_event.Add(delegate);
		}

	private:
		UpscalerType upscaler_type;
		UpscalerDisabledEvent upscaler_disabled_event;
		Uint32 display_width, display_height;

	private:
		virtual void GroupGUI() override;
	};
}

