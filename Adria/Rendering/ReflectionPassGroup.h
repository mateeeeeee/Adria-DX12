#pragma once
#include "PostEffect.h"

namespace adria
{
	enum class ReflectionType : Uint8;
	class GfxDevice;

	class ReflectionPassGroup final : public PostEffectGroup
	{
	public:
		ReflectionPassGroup(GfxDevice* gfx, Uint32 width, Uint32 height);

	private:
		ReflectionType reflection_type;
		Bool is_rtr_supported;

	private:
		virtual void GroupGUI() override;
	};
}