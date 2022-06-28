#pragma once
#include "Enums.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace adria
{
	class RootSignatureCache
	{
	public:

	private:
	};

	class GraphicsPipelineState
	{

	};

	class GraphicsPipelineStateBuilder
	{
	public:
		GraphicsPipelineStateBuilder() = default;
		~GraphicsPipelineStateBuilder() = default;


	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	};

	class ComputePipelineState
	{

	};

	class ComputePipelineStateBuilder
	{
	public:
		ComputePipelineStateBuilder() = default;
		~ComputePipelineStateBuilder() = default;

	private:
	};

}