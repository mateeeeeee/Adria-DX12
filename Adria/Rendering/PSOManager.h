#pragma once
#include "Enums.h"
#include "ShaderManager.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace adria
{
	class PSOManager
	{
	public:
		static void Initialize(GraphicsDevice* gfx);
		static void Destroy();

		static ID3D12RootSignature* GetRootSignature(ERootSignature root_sig);
		static ID3D12PipelineState* GetPipelineState(EPipelineStateObject pso);
	};

}