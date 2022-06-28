#pragma once
#include "Enums.h"

struct ID3D12RootSignature;

namespace adria
{
	class GraphicsDevice;

	namespace RootSignatureCache
	{
		void Initialize(GraphicsDevice* gfx);
		void Destroy();
		ID3D12RootSignature* Get(ERootSignature root_signature_id);
	};
}