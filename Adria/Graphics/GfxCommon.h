#pragma once
#include "GfxDescriptor.h"

namespace adria
{
	class GfxDevice;
	class GfxTexture;

	enum class GfxCommonTextureType : uint8
	{
		WhiteTexture2D,
		BlackTexture2D,
		DefaultNormal2D,
		MetallicRoughness2D,
		Count
	};

	enum class GfxCommonViewType : uint8
	{
		NullTexture2D_SRV,
		NullTexture2D_UAV,
		NullTextureCube_SRV,
		NullTexture2DArray_SRV,
		WhiteTexture2D_SRV,
		BlackTexture2D_SRV,
		DefaultNormal2D_SRV,
		MetallicRoughness2D_SRV,
		Count
	};

	namespace gfxcommon
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();

		GfxTexture*   GetCommonTexture(GfxCommonTextureType);
		GfxDescriptor GetCommonView(GfxCommonViewType);
	}
}