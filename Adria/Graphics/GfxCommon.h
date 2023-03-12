#pragma once
#include <d3d12.h>
#include "../Core/Definitions.h"

namespace adria
{
	class GfxDevice;
	class GfxTexture;

	enum class GfxCommonTextureType : uint8
	{
		WhiteTexture2D,
		BlackTexture2D,
		Count
	};

	enum class GfxCommonViewType : uint8
	{
		NullTexture2D_SRV,
		NullTexture2D_UAV,
		NullTextureCube_SRV,
		NullTexture2DArray_SRV,
		Count
	};

	namespace gfxcommon
	{
		void Initialize(GfxDevice* gfx);
		void Destroy();

		GfxTexture* GetCommonTexture(GfxCommonTextureType);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCommonView(GfxCommonViewType);
	}
}