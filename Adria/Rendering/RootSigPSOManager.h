#pragma once
#include "Enums.h"

struct ID3D12Device;
struct ID3D12RootSignature;
struct ID3D12PipelineState;


namespace adria
{
	class RootSigPSOManager
	{
	public:
		static void Initialize(ID3D12Device* gfx);
		static void Destroy();

		static ID3D12RootSignature* GetRootSignature(ERootSignature root_sig);
		static ID3D12PipelineState* GetPipelineState(EPipelineStateObject pso);

		static void CheckIfShadersHaveChanged();
	};

}