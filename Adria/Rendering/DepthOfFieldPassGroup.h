#pragma once
#include "PostEffect.h"

namespace adria
{
	enum class DepthOfFieldType : Uint8;

	class GfxDevice;
	class DepthOfFieldPassGroup final : public PostEffectGroup
	{
	public:
		DepthOfFieldPassGroup(GfxDevice* gfx, Uint32 width, Uint32 height);

	private:
		DepthOfFieldType depth_of_field_type;

	private:
		virtual void GroupGUI() override;

	};
}