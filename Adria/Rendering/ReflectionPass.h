#pragma once
#include "PostEffect.h"

namespace adria
{
	enum class Reflections : uint8;
	class GfxDevice;

	class ReflectionPass final : public PostEffectGroup
	{
	public:
		ReflectionPass(GfxDevice* gfx, uint32 width, uint32 height);

	private:
		Reflections reflections;
		bool is_rtr_supported;
	private:
		virtual void GroupGUI() override;

	};
}