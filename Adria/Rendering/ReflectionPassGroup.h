#pragma once
#include "PostEffect.h"

namespace adria
{
	enum class ReflectionType : uint8;
	class GfxDevice;

	class ReflectionPassGroup final : public PostEffectGroup
	{
	public:
		ReflectionPassGroup(GfxDevice* gfx, uint32 width, uint32 height);

		virtual bool IsEnabled(PostProcessor const*) const override;

	private:
		ReflectionType reflection_type;
		bool is_rtr_supported;

	private:
		virtual void GroupGUI() override;
	};
}