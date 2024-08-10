#pragma once
#include "PostEffect.h"

namespace adria
{
	enum class DepthOfFieldType : uint8;

	class GfxDevice;
	class DepthOfFieldPassGroup final : public PostEffectGroup
	{
	public:
		DepthOfFieldPassGroup(GfxDevice* gfx, uint32 width, uint32 height);

	private:
		DepthOfFieldType depth_of_field_type;

	private:
		virtual void GroupGUI() override;

	};
}