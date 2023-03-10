#pragma once
#include <d3d12.h>
#include "../Core/Definitions.h"

namespace adria
{
	class GraphicsDevice;
	class Texture;

	enum class ECommonTextureType : uint8
	{
		WhiteTexture2D,
		BlackTexture2D,
		Count
	};

	enum class ECommonViewType : uint8
	{
		NullTexture2D_SRV,
		NullTexture2D_UAV,
		NullTextureCube_SRV,
		NullTexture2DArray_SRV,
		Count
	};

	namespace gfxcommon
	{
		void Initialize(GraphicsDevice* gfx);
		void Destroy();

		Texture* GetCommonTexture(ECommonTextureType);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCommonView(ECommonViewType);
	}
}