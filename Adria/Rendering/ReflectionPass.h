#pragma once
#include "PostEffect.h"

namespace adria
{
	enum class ReflectionType : uint8;
	class GfxDevice;

	class ReflectionPass final : public PostEffectGroup
	{
	public:
		ReflectionPass(GfxDevice* gfx, uint32 width, uint32 height);

	private:
		ReflectionType reflection_type;
		bool is_rtr_supported;
	private:
		virtual void GroupGUI() override;

	};
}