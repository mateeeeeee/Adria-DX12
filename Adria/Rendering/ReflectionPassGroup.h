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

	private:
		ReflectionType reflection_type;
		bool is_rtr_supported;

	private:
		virtual void GroupGUI() override;
	};
}